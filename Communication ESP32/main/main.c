#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_err.h"

#include "audio_pipeline.h"
#include "i2s_audio.h"
#include "bt_audio.h"

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    audio_pipeline_init();
    i2s_audio_init();
    bt_audio_init();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}