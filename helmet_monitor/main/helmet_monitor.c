#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"

////////////////////////////////////////////////////////////
// I2C CONFIG
////////////////////////////////////////////////////////////

#define SDA 5
#define SCL 6
#define I2C_PORT I2C_NUM_0
#define FREQ 400000

////////////////////////////////////////////////////////////
// SENSOR ADDRESSES
////////////////////////////////////////////////////////////

#define LSM6DSO_ADDR 0x6B
#define MS5611_ADDR  0x77
#define MAX30102_ADDR 0x57

////////////////////////////////////////////////////////////
// LSM6DSO REGISTERS
////////////////////////////////////////////////////////////

#define CTRL1_XL 0x10
#define OUTX_L_A 0x28

////////////////////////////////////////////////////////////
// MS5611 COMMANDS
////////////////////////////////////////////////////////////

#define RESET_CMD 0x1E
#define ADC_READ  0x00
#define D1_CONV   0x48
#define D2_CONV   0x58

////////////////////////////////////////////////////////////
// MAX30102 REGISTERS
////////////////////////////////////////////////////////////

#define REG_FIFO_DATA   0x07
#define REG_FIFO_CONFIG 0x08
#define REG_MODE_CONFIG 0x09
#define REG_SPO2_CONFIG 0x0A
#define REG_LED1_PA     0x0C
#define REG_LED2_PA     0x0D

////////////////////////////////////////////////////////////
// GLOBAL FLAGS
////////////////////////////////////////////////////////////

volatile int shock_flag = 0;
volatile int altitude_flag = 0;
volatile int stillness_flag = 0;
volatile int fall_confirmed_flag = 0;

////////////////////////////////////////////////////////////
// MS5611 CALIBRATION STORAGE
////////////////////////////////////////////////////////////

uint16_t C[7];

////////////////////////////////////////////////////////////
// I2C INIT
////////////////////////////////////////////////////////////

void i2c_init()
{
    i2c_config_t conf =
    {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA,
        .scl_io_num = SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = FREQ
    };

    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}

////////////////////////////////////////////////////////////
// I2C HELPERS
////////////////////////////////////////////////////////////

void write_reg(uint8_t addr,uint8_t reg,uint8_t val)
{
    uint8_t data[2]={reg,val};
    i2c_master_write_to_device(I2C_PORT,addr,data,2,100);
}

void read_bytes(uint8_t addr,uint8_t reg,uint8_t *data,int len)
{
    i2c_master_write_read_device(I2C_PORT,addr,&reg,1,data,len,100);
}

////////////////////////////////////////////////////////////
// IMU INIT
////////////////////////////////////////////////////////////

void imu_init()
{
    uint8_t config[2]={CTRL1_XL,0x48};

    i2c_master_write_to_device(
    I2C_PORT,
    LSM6DSO_ADDR,
    config,
    2,
    100);
}

////////////////////////////////////////////////////////////
// MS5611 INIT
////////////////////////////////////////////////////////////

void ms5611_init()
{
    uint8_t cmd=RESET_CMD;

    i2c_master_write_to_device(
    I2C_PORT,
    MS5611_ADDR,
    &cmd,
    1,
    100);

    vTaskDelay(pdMS_TO_TICKS(20));

    for(int i=0;i<6;i++)
    {
        uint8_t reg=0xA2+(i*2);
        uint8_t data[2];

        read_bytes(
        MS5611_ADDR,
        reg,
        data,
        2);

        C[i+1]=(data[0]<<8)|data[1];
    }
}

////////////////////////////////////////////////////////////
// READ MS5611 ADC
////////////////////////////////////////////////////////////

uint32_t read_adc()
{
    uint8_t data[3];

    read_bytes(
    MS5611_ADDR,
    ADC_READ,
    data,
    3);

    return (data[0]<<16)|
           (data[1]<<8)|
            data[2];
}

////////////////////////////////////////////////////////////
// MAX30102 INIT
////////////////////////////////////////////////////////////

void max30102_init()
{
    printf("[PULSE] Initializing sensor...\n");

    write_reg(MAX30102_ADDR,
              REG_MODE_CONFIG,
              0x40);

    vTaskDelay(pdMS_TO_TICKS(100));

    write_reg(MAX30102_ADDR,
              REG_FIFO_CONFIG,
              0x0F);

    write_reg(MAX30102_ADDR,
              REG_SPO2_CONFIG,
              0x27);

    write_reg(MAX30102_ADDR,
              REG_LED1_PA,
              0x24);

    write_reg(MAX30102_ADDR,
              REG_LED2_PA,
              0x24);

    write_reg(MAX30102_ADDR,
              REG_MODE_CONFIG,
              0x03);

    printf("[PULSE] Ready\n");
}

////////////////////////////////////////////////////////////
// READ IR SAMPLE
////////////////////////////////////////////////////////////

uint32_t read_ir_sample()
{
    uint8_t data[3];

    read_bytes(
    MAX30102_ADDR,
    REG_FIFO_DATA,
    data,
    3);

    return (data[0]<<16)|
           (data[1]<<8)|
            data[2];
}

////////////////////////////////////////////////////////////
// BPM ESTIMATION
////////////////////////////////////////////////////////////

int compute_bpm(uint32_t *samples,int count)
{
    int peaks=0;

    for(int i=2;i<count-2;i++)
    {
        if(samples[i]>samples[i-1] &&
           samples[i]>samples[i+1] &&
           samples[i]>samples[i-2] &&
           samples[i]>samples[i+2] &&
           samples[i]>70000)
        peaks++;
    }

    return peaks*2;
}

////////////////////////////////////////////////////////////
// IMU TASK
////////////////////////////////////////////////////////////

void imu_task(void *arg)
{
    imu_init();

    uint8_t data[6];

    while(1)
    {
        read_bytes(
        LSM6DSO_ADDR,
        OUTX_L_A,
        data,
        6);

        int16_t ax=data[1]<<8|data[0];
        int16_t ay=data[3]<<8|data[2];
        int16_t az=data[5]<<8|data[4];

        float magnitude=
        sqrt(
        pow(ax*0.000122,2)+
        pow(ay*0.000122,2)+
        pow(az*0.000122,2));

        if(magnitude>3)
        {
            shock_flag=1;

            printf("⚠️ IMPACT DETECTED\n");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

////////////////////////////////////////////////////////////
// BAROMETER TASK
////////////////////////////////////////////////////////////

void baro_task(void *arg)
{
    ms5611_init();

    float baseline=0;

    for(int i=0;i<10;i++)
    {
        uint32_t D1,D2;

        uint8_t cmd=D1_CONV;
        i2c_master_write_to_device(I2C_PORT,MS5611_ADDR,&cmd,1,100);

        vTaskDelay(pdMS_TO_TICKS(10));

        D1=read_adc();

        cmd=D2_CONV;
        i2c_master_write_to_device(I2C_PORT,MS5611_ADDR,&cmd,1,100);

        vTaskDelay(pdMS_TO_TICKS(10));

        D2=read_adc();

        int32_t dT=D2-((uint32_t)C[5]<<8);

        int64_t OFF=((int64_t)C[2]<<16)+(((int64_t)dT*C[4])>>7);
        int64_t SENS=((int64_t)C[1]<<15)+(((int64_t)dT*C[3])>>8);

        int32_t P=(((D1*SENS)>>21)-OFF)>>15;

        float altitude=
        44330*(1-pow(P/101325.0,0.1903));

        baseline+=altitude;
    }

    baseline/=10;

    while(1)
    {
        uint32_t D1,D2;

        uint8_t cmd=D1_CONV;
        i2c_master_write_to_device(I2C_PORT,MS5611_ADDR,&cmd,1,100);

        vTaskDelay(pdMS_TO_TICKS(10));

        D1=read_adc();

        cmd=D2_CONV;
        i2c_master_write_to_device(I2C_PORT,MS5611_ADDR,&cmd,1,100);

        vTaskDelay(pdMS_TO_TICKS(10));

        D2=read_adc();

        int32_t dT=D2-((uint32_t)C[5]<<8);

        int64_t OFF=((int64_t)C[2]<<16)+(((int64_t)dT*C[4])>>7);
        int64_t SENS=((int64_t)C[1]<<15)+(((int64_t)dT*C[3])>>8);

        int32_t P=(((D1*SENS)>>21)-OFF)>>15;

        float altitude=
        44330*(1-pow(P/101325.0,0.1903));

        if(fabs(altitude-baseline)>1.5)
        {
            altitude_flag=1;

            printf("⚠️ ALTITUDE CHANGE DETECTED\n");
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

////////////////////////////////////////////////////////////
// STILLNESS TASK
////////////////////////////////////////////////////////////

void stillness_task(void *arg)
{
    while(1)
    {
        if(shock_flag && altitude_flag)
        {
            printf("⏳ Checking movement stability...\n");

            vTaskDelay(pdMS_TO_TICKS(5000));

            stillness_flag=1;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

////////////////////////////////////////////////////////////
// SUPERVISOR TASK
////////////////////////////////////////////////////////////

void supervisor_task(void *arg)
{
    while(1)
    {
        if(shock_flag &&
           altitude_flag &&
           stillness_flag)
        {
            printf("\n🚨🚨 FALL CONFIRMED 🚨🚨\n");

            fall_confirmed_flag=1;

            shock_flag=0;
            altitude_flag=0;
            stillness_flag=0;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

////////////////////////////////////////////////////////////
// PULSE TASK
////////////////////////////////////////////////////////////

void pulse_task(void *arg)
{
    max30102_init();

    uint32_t samples[300];

    while(1)
    {
        if(fall_confirmed_flag)
            printf("\n[PULSE] Emergency capture\n");
        else
            printf("\n[PULSE] Scheduled capture\n");

        for(int i=0;i<300;i++)
        {
            samples[i]=read_ir_sample();

            vTaskDelay(pdMS_TO_TICKS(100));
        }

        int bpm=compute_bpm(samples,300);

        printf("[PULSE] BPM = %d\n",bpm);

        fall_confirmed_flag=0;

        vTaskDelay(pdMS_TO_TICKS(600000));
    }
}

////////////////////////////////////////////////////////////
// MAIN
////////////////////////////////////////////////////////////

void app_main()
{
    printf("\nHelmet Monitoring Engine Started\n");

    i2c_init();

    xTaskCreate(imu_task,"imu",4096,NULL,4,NULL);
    xTaskCreate(baro_task,"baro",4096,NULL,3,NULL);
    xTaskCreate(stillness_task,"still",4096,NULL,2,NULL);
    xTaskCreate(supervisor_task,"logic",4096,NULL,2,NULL);
    xTaskCreate(pulse_task,"pulse",4096,NULL,1,NULL);
}