#include "bt_audio.h"
#include "i2s_audio.h"
#include "audio_pipeline.h"
#include "nvs_flash.h"

void app_main(void)
{
    nvs_flash_init();

    audio_pipeline_init();
    i2s_audio_init();
    bt_audio_init();
}