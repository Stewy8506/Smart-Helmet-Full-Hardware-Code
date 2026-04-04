

#pragma once

#include <stdint.h>

// Process audio buffer for ANC (Active Noise Cancellation)
void dsp_process(int32_t *buffer, int samples);