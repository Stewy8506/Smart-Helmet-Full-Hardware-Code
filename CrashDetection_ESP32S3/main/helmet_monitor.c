#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/uart.h"

////////////////////////////////////////////////////////////
// UART CONFIG
////////////////////////////////////////////////////////////

#define UART_PORT UART_NUM_1
#define UART_TX 17
#define UART_RX 16
#define UART_BAUD_RATE 115200

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

#define LSM6DSO_ADDR    0x6B
#define MS5611_ADDR     0x77
#define MAX30102_ADDR   0x57
#define TMP117_ADDR     0x48

////////////////////////////////////////////////////////////
// REGISTERS
////////////////////////////////////////////////////////////

#define CTRL1_XL        0x10
#define OUTX_L_A        0x28

#define RESET_CMD       0x1E
#define ADC_READ        0x00
#define D1_CONV         0x48
#define D2_CONV         0x58

#define REG_FIFO_DATA   0x07
#define REG_FIFO_CONFIG 0x08
#define REG_MODE_CONFIG 0x09
#define REG_SPO2_CONFIG 0x0A
#define REG_LED1_PA     0x0C
#define REG_LED2_PA     0x0D

#define TMP117_TEMP_REG     0x00
#define TMP117_CONFIG_REG   0x01

////////////////////////////////////////////////////////////
// VALIDATION LIMITS
////////////////////////////////////////////////////////////

#define BPM_MIN_VALID 75
#define BPM_MAX_VALID 130
#define TEMP_MIN_VALID 35.0
#define TEMP_MAX_VALID 38.0
#define IR_THRESHOLD 70000

////////////////////////////////////////////////////////////
// FLAGS
////////////////////////////////////////////////////////////

volatile int shock_flag = 0;
volatile int altitude_flag = 0;
volatile int stillness_flag = 0;
volatile int fall_confirmed_flag = 0;

uint16_t C[7];

////////////////////////////////////////////////////////////
// UART INIT
////////////////////////////////////////////////////////////

void uart_init()
{
    uart_config_t uart_config =
    {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_driver_install(UART_PORT, 1024, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, UART_TX, UART_RX,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);

    printf("[UART] Initialized\n");
}

////////////////////////////////////////////////////////////
// UART SEND PACKET
////////////////////////////////////////////////////////////

void send_uart_packet(int fall, int bpm, float temp)
{
    char buffer[64];

    sprintf(buffer,
            "FALL:%d BPM:%d TEMP:%d\n",
            fall,
            bpm,
            (int)temp);

    uart_write_bytes(UART_PORT, buffer, strlen(buffer));

    printf("[UART SENT] %s", buffer);
}

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

void write_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    i2c_master_write_to_device(I2C_PORT, addr, data, 2, 100);
}

void read_bytes(uint8_t addr, uint8_t reg, uint8_t *data, int len)
{
    i2c_master_write_read_device(I2C_PORT, addr, &reg, 1, data, len, 100);
}

////////////////////////////////////////////////////////////
// IMU INIT
////////////////////////////////////////////////////////////

void imu_init()
{
    uint8_t config[2] = {CTRL1_XL, 0x48};
    i2c_master_write_to_device(I2C_PORT, LSM6DSO_ADDR, config, 2, 100);
}

////////////////////////////////////////////////////////////
// MAX30102 INIT
////////////////////////////////////////////////////////////

void max30102_init()
{
    write_reg(MAX30102_ADDR, REG_MODE_CONFIG, 0x40);
    vTaskDelay(pdMS_TO_TICKS(100));

    write_reg(MAX30102_ADDR, REG_FIFO_CONFIG, 0x0F);
    write_reg(MAX30102_ADDR, REG_SPO2_CONFIG, 0x27);
    write_reg(MAX30102_ADDR, REG_LED1_PA, 0x24);
    write_reg(MAX30102_ADDR, REG_LED2_PA, 0x24);
    write_reg(MAX30102_ADDR, REG_MODE_CONFIG, 0x03);

    printf("[PULSE] Ready\n");
}

////////////////////////////////////////////////////////////
// READ IR SAMPLE
////////////////////////////////////////////////////////////

uint32_t read_ir_sample()
{
    uint8_t data[3];
    read_bytes(MAX30102_ADDR, REG_FIFO_DATA, data, 3);
    return (data[0] << 16) | (data[1] << 8) | data[2];
}

////////////////////////////////////////////////////////////
// TMP117 INIT
////////////////////////////////////////////////////////////

void tmp117_init()
{
    uint8_t config_data[3];
    uint16_t config_value = 0x00A0;

    config_data[0] = TMP117_CONFIG_REG;
    config_data[1] = config_value >> 8;
    config_data[2] = config_value & 0xFF;

    i2c_master_write_to_device(I2C_PORT,
                               TMP117_ADDR,
                               config_data,
                               3,
                               100);

    printf("[TEMP] TMP117 initialized\n");
}

////////////////////////////////////////////////////////////
// READ TEMPERATURE
////////////////////////////////////////////////////////////

float read_temperature()
{
    uint8_t reg = TMP117_TEMP_REG;
    uint8_t data[2];

    i2c_master_write_read_device(I2C_PORT,
                                TMP117_ADDR,
                                &reg,
                                1,
                                data,
                                2,
                                100);

    int16_t raw_temp = (data[0] << 8) | data[1];

    return raw_temp * 0.0078125;
}

////////////////////////////////////////////////////////////
// BPM COMPUTATION
////////////////////////////////////////////////////////////

int compute_bpm(uint32_t *samples, int count)
{
    int peaks = 0;

    for(int i = 2; i < count - 2; i++)
    {
        if(samples[i] > samples[i-1] &&
           samples[i] > samples[i+1] &&
           samples[i] > samples[i-2] &&
           samples[i] > samples[i+2] &&
           samples[i] > IR_THRESHOLD)
        {
            peaks++;
        }
    }

    int bpm = peaks * 2;

    if(bpm >= BPM_MIN_VALID && bpm <= BPM_MAX_VALID)
        return bpm;

    return -1;
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
        read_bytes(LSM6DSO_ADDR,
                   OUTX_L_A,
                   data,
                   6);

        int16_t ax =
            (data[1] << 8) | data[0];

        int16_t ay =
            (data[3] << 8) | data[2];

        int16_t az =
            (data[5] << 8) | data[4];

        float magnitude =
            sqrt(pow(ax*0.000122,2)+
                 pow(ay*0.000122,2)+
                 pow(az*0.000122,2));

        if(magnitude > 3)
        {
            shock_flag = 1;
            printf("IMPACT\n");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

////////////////////////////////////////////////////////////
// STILLNESS TASK
////////////////////////////////////////////////////////////

void stillness_task(void *arg)
{
    while(1)
    {
        if(shock_flag)
        {
            vTaskDelay(pdMS_TO_TICKS(5000));
            stillness_flag = 1;
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
        if(shock_flag && stillness_flag)
        {
            fall_confirmed_flag = 1;

            float temp =
                read_temperature();

            send_uart_packet(1,0,temp);

            shock_flag=0;
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
    tmp117_init();

    uint32_t samples[300];

    while(1)
    {
        for(int i=0;i<300;i++)
        {
            samples[i]=
                read_ir_sample();

            vTaskDelay(pdMS_TO_TICKS(100));
        }

        int bpm=
            compute_bpm(samples,300);

        float temp=
            read_temperature();

        send_uart_packet(
            fall_confirmed_flag,
            bpm,
            temp
        );

        fall_confirmed_flag=0;

        vTaskDelay(pdMS_TO_TICKS(600000));
    }
}

////////////////////////////////////////////////////////////
// MAIN
////////////////////////////////////////////////////////////

void app_main()
{
    printf("HELMET ENGINE STARTED\n");

    i2c_init();

    uart_init();

    xTaskCreate(
        imu_task,
        "IMU",
        4096,
        NULL,
        4,
        NULL
    );

    xTaskCreate(
        stillness_task,
        "STILLNESS",
        4096,
        NULL,
        2,
        NULL
    );

    xTaskCreate(
        supervisor_task,
        "SUPERVISOR",
        4096,
        NULL,
        2,
        NULL
    );

    xTaskCreate(
        pulse_task,
        "PULSE",
        8192,
        NULL,
        1,
        NULL
    );
}
