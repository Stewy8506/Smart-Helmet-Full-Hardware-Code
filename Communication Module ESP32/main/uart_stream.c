#include "uart_stream.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

#define UART_STREAM_PORT UART_NUM_2
#define UART_STREAM_TX_PIN 17
#define UART_STREAM_RX_PIN 16
#define UART_STREAM_BAUD 1500000

#define UART_SYNC_WORD 0xAA55

static const char *TAG = "UART_STREAM";

// Internal TX buffer
static uint8_t tx_buffer[4096];

void uart_stream_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_STREAM_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_driver_install(UART_STREAM_PORT, 8192, 0, 0, NULL, 0);
    uart_param_config(UART_STREAM_PORT, &uart_config);

    uart_set_pin(UART_STREAM_PORT,
                 UART_STREAM_TX_PIN,
                 UART_STREAM_RX_PIN,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "UART TX initialized");
}

void uart_stream_send(int16_t *data, int samples)
{
    static int counter = 0;

    if (++counter % 50 == 0)
    {
        printf("TX %d samples: ", samples);
        for (int i = 0; i < 6 && i < samples; i++)
        {
            printf("%d ", data[i]);
        }
        printf("\n");

        int16_t min = 32767, max = -32768;
        for (int i = 0; i < samples; i++)
        {
            if (data[i] < min) min = data[i];
            if (data[i] > max) max = data[i];
        }
        printf("TX Min: %d Max: %d\n", min, max);
    }

    uint16_t data_bytes = samples * sizeof(int16_t);

    uint16_t sync = UART_SYNC_WORD;
    uint16_t len  = data_bytes;

    int offset = 0;

    memcpy(&tx_buffer[offset], &sync, 2);
    offset += 2;

    memcpy(&tx_buffer[offset], &len, 2);
    offset += 2;

    memcpy(&tx_buffer[offset], data, data_bytes);
    offset += data_bytes;

    uart_write_bytes(UART_STREAM_PORT, (const char *)tx_buffer, offset);
}
