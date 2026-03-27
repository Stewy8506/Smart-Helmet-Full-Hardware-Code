#include "bt_audio.h"
#include "audio_pipeline.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_hf_client_api.h"
#include "esp_log.h"

#define TAG "BT_AUDIO"

static void a2dp_data_cb(const uint8_t *data, uint32_t len)
{
    audio_pipeline_send(data, len);
}

static void a2dp_event_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    ESP_LOGI(TAG, "A2DP event: %d", event);
}

static void hfp_event_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "HFP event: %d", event);
}

void bt_audio_init(void)
{
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BTDM);

    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_bt_gap_set_device_name("Nexus Smart Helmet");

    esp_a2d_register_callback(a2dp_event_cb);
    esp_a2d_sink_register_data_callback(a2dp_data_cb);
    esp_a2d_sink_init();

    esp_hf_client_register_callback(hfp_event_cb);
    esp_hf_client_init();

    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
}