#pragma once
#include <stdint.h>
#include <stddef.h>
#include "driver/i2s_std.h"

extern i2s_chan_handle_t tx_handle;

void i2s_audio_init(void);
void i2s_set_sample_rate(uint32_t rate);
void i2s_audio_write(const void *data, size_t len, size_t *bytes_written);