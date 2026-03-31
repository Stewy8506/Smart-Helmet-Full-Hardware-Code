#include <string.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "CENTRAL_NODE";

typedef struct {
    bool fall_detected;
    float confidence;
    int heart_rate;
    float eda;
} fall_packet_t;


void receive_callback(const esp_now_recv_info_t *recv_info,
                      const uint8_t *incomingData,
                      int len)
{
    fall_packet_t packet;

    memcpy(&packet, incomingData, sizeof(packet));

    ESP_LOGI(TAG, "Fall: %d", packet.fall_detected);
    ESP_LOGI(TAG, "Confidence: %.2f", packet.confidence);
    ESP_LOGI(TAG, "Heart Rate: %d", packet.heart_rate);
    ESP_LOGI(TAG, "EDA: %.2f", packet.eda);
}


void wifi_init()
{
    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA)
    );

    ESP_ERROR_CHECK(
        esp_wifi_start()
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_channel(1,
        WIFI_SECOND_CHAN_NONE)
    );
}


void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();

    ESP_ERROR_CHECK(esp_now_init());

    ESP_ERROR_CHECK(
        esp_now_register_recv_cb(receive_callback)
    );

    ESP_LOGI(TAG, "Receiver ready...");
}