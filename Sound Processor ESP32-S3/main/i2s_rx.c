#include "i2s_rx.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#define TAG "I2S_RX"

static i2s_chan_handle_t rx_handle;
static i2s_chan_handle_t tx_handle;

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 1024

void i2s_rx_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);

    i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = 4,
            .ws   = 5,
            .dout = 18, // for speaker out
            .din  = 19, // from ESP32
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    i2s_channel_init_std_mode(tx_handle, &std_cfg);

    i2s_channel_enable(rx_handle);
    i2s_channel_enable(tx_handle);

    ESP_LOGI(TAG, "I2S RX initialized");
}

static void i2s_rx_task(void *arg)
{
    uint8_t buffer[BUFFER_SIZE];
    size_t bytes_read, bytes_written;

    while (1)
    {
        i2s_channel_read(rx_handle, buffer, BUFFER_SIZE, &bytes_read, portMAX_DELAY);

        // pass-through (can add DSP later)
        i2s_channel_write(tx_handle, buffer, bytes_read, &bytes_written, portMAX_DELAY);
    }
}

void i2s_rx_task_start(void)
{
    xTaskCreatePinnedToCore(i2s_rx_task, "i2s_rx_task", 4096, NULL, 5, NULL, 0);
}