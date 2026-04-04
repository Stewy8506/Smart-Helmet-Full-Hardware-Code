#pragma once
#include <stdint.h>

void i2s_out_init(void);
void i2s_out_write(int16_t *music, int16_t *anc, int count);