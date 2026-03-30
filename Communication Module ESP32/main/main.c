#include <string.h>
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"

#include "bt_audio.h"
#include "uart_stream.h"
#include "resampler.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "MAIN";

typedef struct
{
    uint32_t len;
    uint8_t data[4096]; // must fit full A2DP packet (~3584 bytes)
} audio_packet_t;

static QueueHandle_t audio_queue;

static void audio_pipeline_callback(const uint8_t *data, uint32_t len)
{
    // Allocate packet dynamically (avoid large stack usage in BT thread)
    audio_packet_t *pkt = malloc(sizeof(audio_packet_t));
    if (!pkt)
    {
        return; // drop if allocation fails
    }

    if (len > sizeof(pkt->data))
    {
        len = sizeof(pkt->data);
    }

    pkt->len = len;
    memcpy(pkt->data, data, len);

    // Non-blocking send
    if (xQueueSend(audio_queue, &pkt, 0) != pdTRUE)
    {
        free(pkt); // free if queue full
    }
}

static void audio_task(void *arg)
{
    audio_packet_t *pkt;

    static int16_t out_buffer[2048]; // move to static to avoid stack usage
    while (1)
    {
        if (xQueueReceive(audio_queue, &pkt, portMAX_DELAY))
        {
            // Convert bytes → int16 samples → mono frames (L channel only)
            int16_t *pcm = (int16_t *)pkt->data;
            int total_samples = pkt->len / 2; // int16 samples
            int frames = total_samples / 2;   // stereo → mono frames

            // Extract left channel only to temporary buffer
            static int16_t mono_buffer[2048];
            for (int i = 0; i < frames; i++)
            {
                mono_buffer[i] = pcm[i * 2]; // take L channel
            }

            // TEMP DEBUG: bypass resampler to verify audio path
            int out_samples = resample_44k_to_16k(
                mono_buffer,
                frames,
                out_buffer);


            uart_stream_send(out_buffer, out_samples);
            free(pkt);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32 A2DP → UART transmitter");

    // 🔧 NVS (required for Bluetooth)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 🔧 UART init
    ESP_LOGI(TAG, "Initializing UART...");
    uart_stream_init();

    // 🔧 Resampler init (if needed)
    ESP_LOGI(TAG, "Initializing resampler...");
    resampler_init();

    // 🔧 Create audio queue + task
    audio_queue = xQueueCreate(10, sizeof(audio_packet_t *));
    xTaskCreate(audio_task, "audio_task", 8192, NULL, 5, NULL);

    // 🔧 Bluetooth A2DP init
    ESP_LOGI(TAG, "Initializing Bluetooth A2DP...");
    bt_audio_init(audio_pipeline_callback);

    ESP_LOGI(TAG, "System ready. Waiting for Bluetooth audio...");
}