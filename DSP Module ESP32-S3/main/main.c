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
    ESP_LOGI(TAG, "ESP32-S3 DSP Audio Mixed Output");

    uart_audio_init();
    audio_buffer_init();
    mic_init();
    i2s_out_init();

    int32_t mic_samples[256];
    int16_t anc_samples[256];
    int16_t music_samples[256];

    #define FRAME_SAMPLES 256

    while (1)
    {
        int count = mic_read(mic_samples, FRAME_SAMPLES);

        // Convert processed mic data to 16-bit for ANC mix
        for (int i = 0; i < count; i++)
        {
            anc_samples[i] = (int16_t)mic_samples[i];
        }

        // Pad if needed
        if (count < FRAME_SAMPLES)
        {
            memset(anc_samples + count, 0, (FRAME_SAMPLES - count) * sizeof(int16_t));
        }

        // Read music from UART buffer
        int music_count = audio_buffer_read(music_samples, FRAME_SAMPLES);
        if (music_count < FRAME_SAMPLES)
        {
            memset(music_samples + music_count, 0, (FRAME_SAMPLES - music_count) * sizeof(int16_t));
        }

        // Debug
        static int counter = 0;
        if (++counter % 50 == 0)
        {
            int min = 32767;
            int max = -32768;

            for (int i = 0; i < FRAME_SAMPLES; i++)
            {
                if (anc_samples[i] < min) min = anc_samples[i];
                if (anc_samples[i] > max) max = anc_samples[i];
            }

            printf("MIC ANC Min: %d Max: %d\n", min, max);
        }

        i2s_out_write(music_samples, anc_samples, FRAME_SAMPLES);
    }
}