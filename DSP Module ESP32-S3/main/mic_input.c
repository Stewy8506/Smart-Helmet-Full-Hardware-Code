#include "mic_input.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include <math.h>

void dsp_process(int32_t *buffer, int samples);

#define I2S_PORT I2S_NUM_0

#define I2S_BCK_IO   17   // SCK
#define I2S_WS_IO    18   // WS
#define I2S_DO_IO    -1   // Not used
#define I2S_DI_IO    3    // L/R (SD pin)

static const char *TAG = "MIC_INPUT";

static i2s_chan_handle_t rx_chan_handle = NULL;

static float dsp_gain = 2.0f;
static int dsp_threshold = 500;
static float dsp_alpha = 0.4f;
static int32_t dsp_prev = 0;

void mic_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DI_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan_handle));

    ESP_LOGI(TAG, "I2S STD microphone initialized");
}

// Example read function
int mic_read(int32_t *buffer, size_t samples)
{
    size_t bytes_read = 0;
    esp_err_t result = i2s_channel_read(rx_chan_handle, buffer, samples * sizeof(int32_t), &bytes_read, portMAX_DELAY);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "I2S read failed");
        return 0;
    }

    int samples_read = bytes_read / sizeof(int32_t);

    // DSP pipeline: normalize, gain, noise gate, low-pass filter, RMS
    float rms_acc = 0.0f;

    for (int i = 0; i < samples_read; i++) {
        // Step 1: Normalize (24-bit alignment)
        int32_t sample = buffer[i] >> 14;

        // Step 2: Gain
        sample = (int32_t)(sample * dsp_gain);

        // Clip
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;

        // Step 3: Noise gate
        if (abs(sample) < dsp_threshold) {
            sample = 0;
        }

        // Step 4: Low-pass filter
        sample = (int32_t)(dsp_alpha * sample + (1.0f - dsp_alpha) * dsp_prev);
        dsp_prev = sample;

        // Store back
        buffer[i] = sample;

        // Step 5: RMS accumulation
        rms_acc += (float)(sample * sample);
    }

    // Send processed signal to DSP (ANC stage)
    dsp_process(buffer, samples_read);

    float rms = sqrtf(rms_acc / samples_read);
    ESP_LOGI(TAG, "RMS: %.2f", rms);

    return samples_read;
}