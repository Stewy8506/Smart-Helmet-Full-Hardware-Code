#include <stdint.h>

#define DELAY_SAMPLES 32

static int32_t delay_buffer[DELAY_SAMPLES] = {0};
static int delay_index = 0;

// ANC gain (tune this)
static float anc_gain = 0.8f;

void dsp_process(int32_t *buffer, int samples)
{
    for (int i = 0; i < samples; i++) {
        // Get delayed sample (simulate acoustic delay)
        int32_t delayed = delay_buffer[delay_index];

        // Store current sample into delay buffer
        delay_buffer[delay_index] = buffer[i];

        // Circular buffer index
        delay_index++;
        if (delay_index >= DELAY_SAMPLES) {
            delay_index = 0;
        }

        // Invert + apply gain
        int32_t output = (int32_t)(-delayed * anc_gain);

        // Clip to safe range
        if (output > 32767) output = 32767;
        if (output < -32768) output = -32768;

        // Replace buffer with ANC signal
        buffer[i] = output;
    }
}