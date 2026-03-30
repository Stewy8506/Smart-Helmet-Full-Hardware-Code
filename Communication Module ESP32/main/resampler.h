#pragma once
#include <stdint.h>

void resampler_init(void);

// in_samples = number of int16_t values (L,R interleaved)
// returns number of int16_t output samples
int resample_44k_to_16k(int16_t *in, int in_samples, int16_t *out);