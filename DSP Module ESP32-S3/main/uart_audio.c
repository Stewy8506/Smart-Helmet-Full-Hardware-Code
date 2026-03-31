#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uart_audio.h"
#include "audio_buffer.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

#define UART_NUM UART_NUM_2
#define RX_PIN 15 // RX pin (ESP32-S3 GPIO15)
#define TX_PIN 16 // not used

#define BUF_SIZE 4096

static const char *TAG = "UART_AUDIO";

static void uart_rx_task(void *arg)
{
    uint8_t buf[1024];

    while (1)
    {
        int len = uart_read_bytes(UART_NUM, buf, sizeof(buf), pdMS_TO_TICKS(20));

        if (len > 0)
        {
            static uint8_t packet_buf[4096];
            static int packet_index = 0;

            int offset = 0;
            // Append incoming bytes to packet buffer
            for (int i = 0; i < len; i++)
            {
                packet_buf[packet_index++] = buf[i];

                // Prevent overflow
                if (packet_index >= sizeof(packet_buf))
                {
                    packet_index = 0;
                    offset = 0;
                }
            }

            // Try to parse packets

            while (packet_index - offset >= 4)
            {
                uint16_t sync = packet_buf[offset] | (packet_buf[offset + 1] << 8);

                // Check sync word (same as ESP32 sender)
                if (sync != 0xAA55)
                {
                    offset++;
                    continue;
                }

                uint16_t length = packet_buf[offset + 2] | (packet_buf[offset + 3] << 8);

                // Check if full packet available
                if (packet_index - offset < 4 + length)
                {
                    break;
                }

                int samples = length / 2;
                int16_t pcm[512];

                if (samples > 512)
                    samples = 512;

                int out_idx = 0;

                for (int i = 0; i < samples; i++)
                {
                    int idx = offset + 4 + (i * 2);
                    int16_t sample = (int16_t)(packet_buf[idx] | (packet_buf[idx + 1] << 8));
                    pcm[out_idx++] = sample;
                }

                if (out_idx > 0)
                {
                    audio_buffer_write(pcm, out_idx);
                }

                offset += 4 + length;
            }

            // Shift remaining data to start
            if (offset > 0)
            {
                memmove(packet_buf, &packet_buf[offset], packet_index - offset);
                packet_index -= offset;
            }
        }
    }
}

void uart_audio_init(void)
{
    uart_config_t config = {
        .baud_rate = 2000000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    uart_driver_install(UART_NUM, BUF_SIZE * 4, BUF_SIZE * 4, 0, NULL, 0);
    uart_param_config(UART_NUM, &config);
    uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "UART RX initialized");
    xTaskCreate(uart_rx_task, "uart_rx", 8192, NULL, 5, NULL);
}