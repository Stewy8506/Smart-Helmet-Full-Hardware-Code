#include "uart_audio.h"
#include "audio_buffer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_out.h"
#include <string.h>

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 UART Audio Debug");

    audio_buffer_init();
    uart_audio_init();
    i2s_out_init();

    int16_t samples[512];

    #define FRAME_SAMPLES 256

    while (1)
    {
        int count = audio_buffer_read(samples, FRAME_SAMPLES);

        // Ensure fixed frame size
        if (count < FRAME_SAMPLES)
        {
            memset(samples + count, 0, (FRAME_SAMPLES - count) * sizeof(int16_t));
            count = FRAME_SAMPLES;
        }

        // Debug (reduced overhead)
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

            printf("RX Min: %d Max: %d\n", min, max);
        }

        // Always send fixed-size audio
        i2s_out_write(samples, FRAME_SAMPLES);
    }
}