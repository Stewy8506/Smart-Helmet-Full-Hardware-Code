#include "bt_audio.h"
#include "audio_pipeline.h"
#include "i2s_audio.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_hf_client_api.h"

#include "esp_log.h"
#include "esp_hf_client_api.h"


#define TAG "BT_AUDIO"

static bool hfp_audio_active = false;

static void a2dp_data_cb(const uint8_t *data, uint32_t len)
{
    // If call audio is active, ignore A2DP (pause music)
    if (hfp_audio_active)
    {
        return;
    }

    audio_pipeline_send(data, len);
}

static void a2dp_event_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    ESP_LOGI(TAG, "A2DP event: %d", event);
}

static void hfp_audio_data_cb(const uint8_t *data, uint32_t len)
{
    if (!hfp_audio_active)
        return;

    // 8kHz mono → ~16kHz stereo using linear interpolation
    static uint8_t stereo_buf[2048];
    static int16_t prev_sample = 0;
    static bool has_prev = false;

    uint32_t samples = len / 2; // 16-bit mono
    uint32_t out_len = 0;

    for (uint32_t i = 0; i < samples; i++)
    {
        int16_t curr = (int16_t)(data[i * 2] | (data[i * 2 + 1] << 8));

        int16_t interp;
        if (has_prev)
        {
            interp = (prev_sample + curr) / 2; // simple smoothing
        }
        else
        {
            interp = curr;
            has_prev = true;
        }

        // Output interpolated sample (stereo)
        stereo_buf[out_len++] = interp & 0xFF;
        stereo_buf[out_len++] = (interp >> 8) & 0xFF;
        stereo_buf[out_len++] = interp & 0xFF;
        stereo_buf[out_len++] = (interp >> 8) & 0xFF;

        // Output actual sample (stereo)
        stereo_buf[out_len++] = curr & 0xFF;
        stereo_buf[out_len++] = (curr >> 8) & 0xFF;
        stereo_buf[out_len++] = curr & 0xFF;
        stereo_buf[out_len++] = (curr >> 8) & 0xFF;

        prev_sample = curr;

        if (out_len >= sizeof(stereo_buf) - 8)
            break;
    }

    audio_pipeline_send(stereo_buf, out_len);
}

static void hfp_event_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "HFP event: %d", event);

    switch (event)
    {
        case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
            ESP_LOGI(TAG, "HFP connection state: %d", param->conn_stat.state);
            break;

        case ESP_HF_CLIENT_AUDIO_STATE_EVT:
            ESP_LOGI(TAG, "HFP audio state: %d", param->audio_stat.state);

            if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED)
            {
                hfp_audio_active = true;
                i2s_set_sample_rate(16000);   // switch to call mode
                ESP_LOGI(TAG, "Call audio ACTIVE");
            }
            else
            {
                hfp_audio_active = false;
                i2s_set_sample_rate(44100);   // back to music
                ESP_LOGI(TAG, "Call audio STOPPED");
            }
            break;

        case ESP_HF_CLIENT_RING_IND_EVT:
            ESP_LOGI(TAG, "Incoming call...");
            break;

        default:
            break;
    }
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
    esp_hf_client_register_data_callback(hfp_audio_data_cb, NULL);

    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
}