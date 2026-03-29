#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2s_rx.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#define TAG "I2S_RX"

static i2s_chan_handle_t rx_handle;
static i2s_chan_handle_t tx_handle;

#define SAMPLE_RATE 44100  // Must match transmitter exactly
#define BUFFER_SIZE 1024


static uint8_t buffer[BUFFER_SIZE];

static void i2s_rx_task(void *arg)
{
    size_t bytes_read = 0;
    size_t bytes_written = 0;

    while (1)
    {
        // Read from ESP32
        i2s_channel_read(rx_handle, buffer, BUFFER_SIZE, &bytes_read, portMAX_DELAY);

        if (bytes_read > 0)
        {
            // Pass through stereo data directly (TX and RX formats now match)
            i2s_channel_write(tx_handle, buffer, bytes_read, &bytes_written, portMAX_DELAY);
        }
    }
}

void i2s_rx_init(void)
{
    // RX uses controller 0 as SLAVE (clock from ESP32)
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);

    // TX must be MASTER on a separate controller (to drive speaker clocks)
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);

    // Create RX (slave) and TX (master) on different controllers
    i2s_new_channel(&rx_chan_cfg, NULL, &rx_handle);
    i2s_new_channel(&tx_chan_cfg, &tx_handle, NULL);

    // RX (ESP32 → S3)
    i2s_std_config_t rx_cfg = {
        // Slave ignores its own clock; must just match expected format
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        // Must match transmitter: 16-bit stereo MSB
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = 4,
            .ws   = 5,
            .dout = I2S_GPIO_UNUSED,
            .din  = 6,
        },
    };

    // TX (S3 → speakers)
    i2s_std_config_t tx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        // TX must match RX timing (MSB standard)
        // Keep TX stereo (for headphones), we will duplicate mono → stereo

        // TX needs its own clock pins (DO NOT reuse RX clock pins)
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            // Using pins 15 (BCLK) and 16 (WS) instead
            .bclk = 15,
            .ws   = 16,
            // These pins must connect to your DAC / amplifier, not the ESP32 TX
            .dout = 36,
            .din  = I2S_GPIO_UNUSED,
        },
    };

    i2s_channel_init_std_mode(rx_handle, &rx_cfg);
    i2s_channel_init_std_mode(tx_handle, &tx_cfg);

    i2s_channel_enable(rx_handle);
    i2s_channel_enable(tx_handle);

    xTaskCreatePinnedToCore(i2s_rx_task, "i2s_rx_task", 4096, NULL, 5, NULL, 0);
}

void i2s_rx_task_start(void)
{
    i2s_rx_init();
}