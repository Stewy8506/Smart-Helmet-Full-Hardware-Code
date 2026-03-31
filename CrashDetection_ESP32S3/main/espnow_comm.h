#pragma once

#include <stdbool.h>

typedef struct
{
    bool fall_detected;
    float confidence;
    int heart_rate;
    float eda;

} fall_packet_t;


void espnow_init(void);

void espnow_send_fall_packet(bool fall,
                            float confidence,
                            int hr,
                            float eda);