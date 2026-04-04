#include "uart_audio.h"
#include "audio_buffer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_out.h"
#include <string.h>
#include "mic_input.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 UART Audio Debug");

    mic_init();
    i2s_out_init();

    int32_t mic_samples[256];
    int16_t samples[256];

    #define FRAME_SAMPLES 256

    while (1)
    {
        int count = mic_read(mic_samples, FRAME_SAMPLES);

        // Convert 24-bit mic data to 16-bit audio
        for (int i = 0; i < count; i++)
        {
            samples[i] = mic_samples[i] >> 8; // 24-bit → 16-bit
        }

        // Pad if needed
        if (count < FRAME_SAMPLES)
        {
            memset(samples + count, 0, (FRAME_SAMPLES - count) * sizeof(int16_t));
            count = FRAME_SAMPLES;
        }

        // Debug
        static int counter = 0;
        if (++counter % 50 == 0)
        {
            int min = 32767;
            int max = -32768;

            for (int i = 0; i < FRAME_SAMPLES; i++)
            {
                if (samples[i] < min) min = samples[i];
                if (samples[i] > max) max = samples[i];
            }

            printf("MIC Min: %d Max: %d\n", min, max);
        }

        static int16_t anc_buffer[FRAME_SAMPLES] = {0}; // placeholder ANC buffer
        i2s_out_write(samples, anc_buffer, FRAME_SAMPLES);
    }
}