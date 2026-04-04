#include "i2s_out.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static i2s_chan_handle_t tx_handle;
static const char *TAG = "I2S_OUT";

void i2s_out_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &tx_handle, NULL);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = 35,
            .ws = 36,
            .dout = 39,
            .din = I2S_GPIO_UNUSED}};

    i2s_channel_init_std_mode(tx_handle, &std_cfg);
    i2s_channel_enable(tx_handle);
}

void i2s_out_write(int16_t *music, int16_t *anc, int count)
{
    static int16_t stereo[1024]; // enough for duplicated samples
    int stereo_count = 0;

    int16_t min = 32767;
    int16_t max = -32768;

    for (int i = 0; i < count; i++)
    {
        int32_t s = music[i] + anc[i]; // mix bluetooth + ANC

        // soft clip (non-linear)
        if (s > 20000)
        {
            s = 20000 + (s - 20000) / 4;
        }
        else if (s < -20000)
        {
            s = -20000 + (s + 20000) / 4;
        }

        int16_t out = (int16_t)s; // reduce volume (prevent clipping)

        if (out < min) min = out;
        if (out > max) max = out;

        stereo[stereo_count++] = out;
        stereo[stereo_count++] = out;
    }

    ESP_LOGI(TAG, "OUT Min: %d Max: %d", min, max);

    size_t bytes_written;
    i2s_channel_write(tx_handle, stereo, stereo_count * sizeof(int16_t), &bytes_written, portMAX_DELAY);
}