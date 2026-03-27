#pragma once

#include <stdint.h>

typedef struct {
    uint8_t *data;
    uint32_t len;
} audio_packet_t;

void audio_pipeline_init(void);
int audio_pipeline_send(const uint8_t *data, uint32_t len);