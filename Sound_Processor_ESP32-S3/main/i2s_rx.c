#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2s_rx.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tusb.h"

#define TAG "I2S_RX"

static i2s_chan_handle_t rx_handle;
static i2s_chan_handle_t tx_handle;

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 102
static void usb_audio_init(void)
{
    tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
    };

    tinyusb_driver_install(&tusb_cfg);

    ESP_LOGI(TAG, "TinyUSB initialized");
}

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
            .din  = 6, // from ESP32
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

    usb_audio_init();
    ESP_LOGI(TAG, "I2S RX + USB initialized");
}

static void i2s_rx_task(void *arg)
{
    uint8_t buffer[BUFFER_SIZE];
    size_t bytes_read, bytes_written;

    while (1)
    {
        i2s_channel_read(rx_handle, buffer, BUFFER_SIZE, &bytes_read, portMAX_DELAY);

        // Send audio to USB (TinyUSB)
        if (tud_ready())
        {
            tud_audio_write(buffer, bytes_read);
        }

        // pass-through (speaker output)
        i2s_channel_write(tx_handle, buffer, bytes_read, &bytes_written, portMAX_DELAY);
    }
}

static void usb_task(void *arg)
{
    while (1)
    {
        tud_task(); // handle USB stack
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void i2s_rx_task_start(void)
{
    xTaskCreatePinnedToCore(i2s_rx_task, "i2s_rx_task", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(usb_task, "usb_task", 4096, NULL, 6, NULL, 1);
}