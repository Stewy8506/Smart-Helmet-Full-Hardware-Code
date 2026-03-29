#include "bt_audio.h"
#include "i2s_audio.h"
#include "audio_pipeline.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_hf_client_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "BT_AUDIO"

static bool hfp_audio_active = false;

// ── A2DP data callback ─────────────────────────────────────────────
static void a2dp_data_cb(const uint8_t *data, uint32_t len)
{
    audio_pipeline_send(data, len);
}

// ── A2DP event callback ────────────────────────────────────────────
static void a2dp_event_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event)
    {
    case ESP_A2D_CONNECTION_STATE_EVT:
        ESP_LOGI(TAG, "A2DP connection state: %d", param->conn_stat.state);
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
        ESP_LOGI(TAG, "A2DP audio state: %d", param->audio_stat.state);
        if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
            ESP_LOGI(TAG, "A2DP streaming STARTED");
        } else {
            ESP_LOGI(TAG, "A2DP streaming STOPPED");
        }
        break;

    case ESP_A2D_AUDIO_CFG_EVT:
    {
        // Phone negotiates SBC codec config — extract sample rate and update I2S
        uint8_t sf = param->audio_cfg.mcc.cie.sbc[0] & 0xF0;
        uint32_t rate = 44100;
        if      (sf == 0x10) rate = 48000;
        else if (sf == 0x20) rate = 44100;
        else if (sf == 0x40) rate = 32000;
        else if (sf == 0x80) rate = 16000;
        ESP_LOGI(TAG, "A2DP audio config, setting sample rate: %lu Hz", rate);
        i2s_set_sample_rate(rate);
        break;
    }

    default:
        ESP_LOGI(TAG, "A2DP event: %d", event);
        break;
    }
}

// ── HFP data callback ──────────────────────────────────────────────
static void hfp_audio_data_cb(const uint8_t *data, uint32_t len)
{
    if (!hfp_audio_active)
        return;

    // Upsample 8kHz mono → 16kHz stereo using linear interpolation
    // Input:  16-bit mono samples at 8kHz
    // Output: 16-bit stereo samples at 16kHz (2× interpolated)
    static uint8_t stereo_buf[2048];
    static int16_t prev_sample = 0;
    static bool has_prev = false;

    uint32_t samples = len / 2;
    uint32_t out_len = 0;

    for (uint32_t i = 0; i < samples; i++)
    {
        int16_t curr = (int16_t)(data[i * 2] | (data[i * 2 + 1] << 8));

        int16_t interp = has_prev ? (prev_sample + curr) / 2 : curr;
        has_prev = true;

        // Interpolated sample — stereo (L+R)
        stereo_buf[out_len++] = interp & 0xFF;
        stereo_buf[out_len++] = (interp >> 8) & 0xFF;
        stereo_buf[out_len++] = interp & 0xFF;
        stereo_buf[out_len++] = (interp >> 8) & 0xFF;

        // Actual sample — stereo (L+R)
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

// ── HFP event callback ─────────────────────────────────────────────
static void hfp_event_cb(esp_hf_client_cb_event_t event,
                         esp_hf_client_cb_param_t *param)
{
    switch (event)
    {
    case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
        ESP_LOGI(TAG, "HFP connection state: %d", param->conn_stat.state);
        break;

    case ESP_HF_CLIENT_AUDIO_STATE_EVT:
        ESP_LOGI(TAG, "HFP audio state: %d", param->audio_stat.state);
        if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED) {
            hfp_audio_active = true;
            i2s_set_sample_rate(16000);
            ESP_LOGI(TAG, "Call audio ACTIVE");
        } else {
            hfp_audio_active = false;
            i2s_set_sample_rate(44100);
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

// ── GAP callback ───────────────────────────────────────────────────
static void bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Paired with: %s", param->auth_cmpl.device_name);
        } else {
            ESP_LOGE(TAG, "Pairing failed, status: %d", param->auth_cmpl.stat);
        }
        break;

    case ESP_BT_GAP_PIN_REQ_EVT:
        ESP_LOGI(TAG, "PIN requested, responding with 0000");
        esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
        esp_bt_pin_code_t pin_code = {'0', '0', '0', '0'};
        esp_bt_gap_set_pin(pin_type, 4, pin_code);
        break;

    default:
        ESP_LOGI(TAG, "GAP event: %d", event);
        break;
    }
}

// ── Init ───────────────────────────────────────────────────────────
void bt_audio_init(void)
{
    // Release BLE memory so Classic BT has enough heap
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    // Override default config mode to Classic-only before init
    // BT_CONTROLLER_INIT_CONFIG_DEFAULT sets mode=BTDM by default,
    // which causes ESP_ERR_INVALID_ARG if you then enable Classic-only
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT;

    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_LOGI(TAG, "Bluedroid enabled");

    // GAP
    ESP_ERROR_CHECK(esp_bt_gap_register_callback(bt_gap_cb));
    ESP_ERROR_CHECK(esp_bt_gap_set_device_name("Nexus Smart Helmet"));

    // A2DP sink
    ESP_ERROR_CHECK(esp_a2d_register_callback(a2dp_event_cb));
    ESP_ERROR_CHECK(esp_a2d_sink_register_data_callback(a2dp_data_cb));
    ESP_ERROR_CHECK(esp_a2d_sink_init());

    // HFP client
    ESP_ERROR_CHECK(esp_hf_client_register_callback(hfp_event_cb));
    ESP_ERROR_CHECK(esp_hf_client_init());
    esp_hf_client_register_data_callback(hfp_audio_data_cb, NULL);

    // Make discoverable after everything is registered
    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,
                                              ESP_BT_GENERAL_DISCOVERABLE));

    ESP_LOGI(TAG, "Bluetooth ready — device: Nexus Smart Helmet");
}