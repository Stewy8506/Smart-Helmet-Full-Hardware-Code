#include <stdio.h>
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "I2S_RX";

// ── Pin assignments ────────────────────────────────────────────────
// Connect ESP32 → ESP32-S3:
//   GPIO26 (BCLK) → GPIO4
//   GPIO25 (WS)   → GPIO5
//   GPIO33 (DOUT) → GPIO6
//   GND           → GND
#define I2S_SLAVE_BCLK  4
#define I2S_SLAVE_WS    5
#define I2S_SLAVE_DIN   6

// 44100 Hz × 2 ch × 2 bytes = 176400 bytes/sec
// 4096 bytes ≈ 23ms per read — large enough to absorb A2DP bursts
#define RX_BUF_SIZE     4096

static i2s_chan_handle_t rx_handle;

// ── I2S slave init ─────────────────────────────────────────────────
static void i2s_rx_init(void)
{
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    rx_chan_cfg.dma_desc_num  = 8;
    rx_chan_cfg.dma_frame_num = 256;

    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_handle));

    i2s_std_config_t rx_std_cfg = {
        // Rate hint for DMA sizing only — actual clock comes from ESP32 master
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),

        // Must match ESP32 transmitter exactly
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
            .slot_mode      = I2S_SLOT_MODE_STEREO,
            .slot_mask      = I2S_STD_SLOT_BOTH,
            .ws_width       = 16,
            .ws_pol         = false,
            .bit_shift      = false,
        },

        .gpio_cfg = {
            .mclk  = I2S_GPIO_UNUSED,
            .bclk  = I2S_SLAVE_BCLK,
            .ws    = I2S_SLAVE_WS,
            .dout  = I2S_GPIO_UNUSED,
            .din   = I2S_SLAVE_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &rx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    ESP_LOGI(TAG, "I2S RX slave ready — BCLK=%d WS=%d DIN=%d",
             I2S_SLAVE_BCLK, I2S_SLAVE_WS, I2S_SLAVE_DIN);
}

// ── RX task ────────────────────────────────────────────────────────
static void i2s_rx_task(void *arg)
{
    static int16_t buffer[RX_BUF_SIZE / sizeof(int16_t)];
    size_t bytes_read = 0;
    uint32_t packet_count = 0;

    ESP_LOGI(TAG, "RX task started, waiting for audio...");

    while (1)
    {
        esp_err_t err = i2s_channel_read(
            rx_handle,
            buffer,
            RX_BUF_SIZE,
            &bytes_read,
            pdMS_TO_TICKS(5000)
        );

        if (err == ESP_ERR_TIMEOUT) {
            // Normal during BT silence or before connection — not an error
            ESP_LOGI(TAG, "Waiting for audio...");
            continue;
        }

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Read error: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (bytes_read == 0)
            continue;

        packet_count++;

        // Print peak amplitude every 100 packets (~2 sec at 44100Hz)
        // Printing every packet floods serial and stalls the task
        if (packet_count % 100 == 0)
        {
            int sample_count = (int)(bytes_read / sizeof(int16_t));
            int16_t peak_l = 0, peak_r = 0;

            for (int i = 0; i < sample_count - 1; i += 2)
            {
                int16_t l = buffer[i]     < 0 ? -buffer[i]     : buffer[i];
                int16_t r = buffer[i + 1] < 0 ? -buffer[i + 1] : buffer[i + 1];
                if (l > peak_l) peak_l = l;
                if (r > peak_r) peak_r = r;
            }

            ESP_LOGI(TAG, "pkt=%lu  bytes=%d  peak L=%-6d R=%-6d",
                     packet_count, (int)bytes_read, peak_l, peak_r);
        }

        // ── TODO: forward audio to your output ────────────────────
        // Wire buffer here to a DAC, amplifier, or second I2S TX:
        //
        // size_t written = 0;
        // i2s_channel_write(dac_tx_handle, buffer, bytes_read, &written,
        //                   pdMS_TO_TICKS(100));
    }
}

// ── Entry point ────────────────────────────────────────────────────
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 I2S Receiver starting...");

    i2s_rx_init();

    xTaskCreate(
        i2s_rx_task,
        "i2s_rx",
        4096,
        NULL,
        5,
        NULL
    );
}