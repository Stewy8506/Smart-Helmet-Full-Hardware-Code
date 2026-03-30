#pragma once
#include <stdint.h>

void audio_buffer_init(void);
void audio_buffer_write(int16_t *data, int samples);
int audio_buffer_read(int16_t *out, int max_samples);