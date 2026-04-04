#pragma once
#include <stddef.h>
#include <stdint.h>

void mic_init(void);
int mic_read(int32_t *buffer, size_t samples);