#include "audio_pipeline.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"

extern i2s_chan_handle_t tx_handle;
#include <stdlib.h>
#include <string.h>

#define AUDIO_QUEUE_SIZE 4
#define I2S_PORT I2S_NUM_0

static QueueHandle_t audio_queue;

static void i2s_task(void *arg)
{
    audio_packet_t pkt;
    size_t written_total;

    while (1)
    {
        if (xQueueReceive(audio_queue, &pkt, portMAX_DELAY))
        {
            written_total = 0;

            while (written_total < pkt.len)
            {
                size_t written = 0;
                i2s_channel_write(tx_handle,
                                  pkt.data + written_total,
                                  pkt.len - written_total,
                                  &written,
                                  10 / portTICK_PERIOD_MS);

                written_total += written;

                if (written == 0)
                    vTaskDelay(1);
            }

            free(pkt.data);
        }
    }
}

void audio_pipeline_init(void)
{
    audio_queue = xQueueCreate(AUDIO_QUEUE_SIZE, sizeof(audio_packet_t));
    xTaskCreatePinnedToCore(i2s_task, "i2s_task", 6144, NULL, 10, NULL, 1);
}

int audio_pipeline_send(const uint8_t *data, uint32_t len)
{
    audio_packet_t pkt;

    pkt.data = malloc(len);
    if (!pkt.data) return -1;

    memcpy(pkt.data, data, len);
    pkt.len = len;

    if (xQueueSend(audio_queue, &pkt, 0) != pdTRUE)
    {
        free(pkt.data);
        return -1;
    }

    return 0;
}