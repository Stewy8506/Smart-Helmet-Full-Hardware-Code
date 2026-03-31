#include <string.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "espnow_comm.h"

static const char *TAG = "ESPNOW_TX";


uint8_t receiver_mac[] =
{0x00,0x70,0x07,0x1D,0x4C,0xB8};


static void send_callback(const uint8_t *mac_addr,
                          esp_now_send_status_t status)
{
    ESP_LOGI(TAG,
             "Send status: %s",
             status == ESP_NOW_SEND_SUCCESS ?
             "Success" : "Fail");
}


static void wifi_init()
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


void espnow_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();

    ESP_ERROR_CHECK(
        esp_now_init()
    );

    ESP_ERROR_CHECK(
        esp_now_register_send_cb(send_callback)
    );


    esp_now_peer_info_t peer = {0};

    memcpy(peer.peer_addr,
           receiver_mac,
           6);

    peer.channel = 1;
    peer.encrypt = false;

    ESP_ERROR_CHECK(
        esp_now_add_peer(&peer)
    );

    ESP_LOGI(TAG, "ESP-NOW ready");
}


void espnow_send_fall_packet(bool fall,
                            float confidence,
                            int hr,
                            float eda)
{
    fall_packet_t packet;

    packet.fall_detected = fall;
    packet.confidence = confidence;
    packet.heart_rate = hr;
    packet.eda = eda;

    esp_now_send(receiver_mac,
                 (uint8_t *)&packet,
                 sizeof(packet));
}