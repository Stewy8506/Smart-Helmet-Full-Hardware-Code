#include "resampler.h"
#include <stdint.h>

#define INPUT_RATE   44100
#define OUTPUT_RATE  16000


// Init resampler
void resampler_init(void)
{
    // no-op (stateless resampler)
}


// Main resampling function
// in: mono PCM
// in_samples: number of samples
// out: output buffer (mono)
// returns: number of output samples
int resample_44k_to_16k(int16_t *in, int in_samples, int16_t *out)
{
    int out_index = 0;

    uint32_t local_pos = 0;
    uint32_t local_step = ((uint32_t)INPUT_RATE << 16) / OUTPUT_RATE;

    while ((local_pos >> 16) < (in_samples - 1)) {

        int idx = local_pos >> 16;
        uint32_t frac = local_pos & 0xFFFF;

        int16_t s1 = in[idx];
        int16_t s2 = in[idx + 1];

        // Safe linear interpolation (32-bit math, use shift to avoid division)
        out[out_index++] = s1 + (((int32_t)(s2 - s1) * frac) >> 16);

        local_pos += local_step;
    }

    return out_index;
}