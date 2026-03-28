#include "i2s_audio.h"
#include "driver/i2s_std.h"

#define I2S_BCK_IO 26
#define I2S_WS_IO 25
#define I2S_DO_IO 33

// Make handle global so audio_pipeline can use it
i2s_chan_handle_t tx_handle;
static uint32_t current_sample_rate = 44100;

void i2s_audio_init(void)
{
    // Create I2S channel (MASTER)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &tx_handle, NULL);

    // Standard I2S configuration
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_IO,
            .ws   = I2S_WS_IO,
            .dout = I2S_DO_IO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    // Initialize and enable channel
    i2s_channel_init_std_mode(tx_handle, &std_cfg);
    i2s_channel_enable(tx_handle);
}

void i2s_set_sample_rate(uint32_t rate)
{
    if (rate == current_sample_rate)
        return;

    // Disable channel before reconfiguring
    i2s_channel_disable(tx_handle);

    // Update clock config
    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate);
    i2s_channel_reconfig_std_clock(tx_handle, &clk_cfg);

    // Re-enable channel
    i2s_channel_enable(tx_handle);

    current_sample_rate = rate;
}