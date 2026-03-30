#include "uart_audio.h"
#include "audio_buffer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 UART Audio Debug");

    audio_buffer_init();
    uart_audio_init();

    int16_t samples[512];

    while (1)
    {
        int count = audio_buffer_read(samples, 512);

        if (count > 0)
        {
            int min = 32767;
            int max = -32768;

            for (int i = 0; i < count; i++)
            {
                if (samples[i] < min)
                    min = samples[i];
                if (samples[i] > max)
                    max = samples[i];
            }

            static int counter = 0;

            if (++counter % 20 == 0)
            {
                printf("RX Min: %d Max: %d\n", min, max);
            }

            if (counter % 20 == 0)
            {
                for (int i = 0; i < 6 && i < count; i++)
                {
                    printf("%d ", samples[i]);
                }

                printf("\nRX Min: %d Max: %d\n", min, max);
            }
        }

        // ALWAYS yield (critical for watchdog)
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}