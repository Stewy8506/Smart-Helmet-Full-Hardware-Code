#include "i2s_rx.h"
#include "esp_log.h"

void app_main(void)
{
    ESP_LOGI("MAIN", "Starting ESP32-S3 audio receiver");

    i2s_rx_init();
    i2s_rx_task_start();
}