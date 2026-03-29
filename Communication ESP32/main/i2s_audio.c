#include "i2s_audio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"   // ← add this
#include "freertos/task.h" 

#define TAG "I2S_TX"

#define I2S_BCK_IO  26
#define I2S_WS_IO   25
#define I2S_DO_IO   33

i2s_chan_handle_t tx_handle;
static uint32_t current_sample_rate = 44100;

void i2s_audio_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(current_sample_rate),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT, // 16-bit slots, no padding
            .slot_mode      = I2S_SLOT_MODE_STEREO,
            .slot_mask      = I2S_STD_SLOT_BOTH,
            .ws_width       = 16,                        // WS toggles every 16 BCLKs
            .ws_pol         = false,
            .bit_shift      = false,                     // left-justified, no offset
        },
        .gpio_cfg = {
            .mclk  = I2S_GPIO_UNUSED,
            .bclk  = I2S_BCK_IO,
            .ws    = I2S_WS_IO,
            .dout  = I2S_DO_IO,
            .din   = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    ESP_LOGI(TAG, "I2S TX initialized at %lu Hz", current_sample_rate);
}

void i2s_set_sample_rate(uint32_t rate)
{
    if (rate == current_sample_rate)
        return;

    ESP_LOGI(TAG, "Updating sample rate: %lu -> %lu", current_sample_rate, rate);

    ESP_ERROR_CHECK(i2s_channel_disable(tx_handle));

    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate);
    ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(tx_handle, &clk_cfg));

    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    current_sample_rate = rate;
}

void i2s_audio_write(const void *data, size_t len, size_t *bytes_written)
{
    esp_err_t err = i2s_channel_write(tx_handle, data, len, bytes_written, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "i2s_channel_write: %s", esp_err_to_name(err));
    }
}

static void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len)
{
    size_t written = 0;
    i2s_audio_write(data, len, &written);
    ESP_LOGI("A2DP", "Received %lu bytes, wrote %zu", len, written);  // ← add this
}