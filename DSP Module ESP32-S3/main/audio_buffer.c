#include "audio_buffer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#define BUFFER_SIZE 8192

static int16_t buffer[BUFFER_SIZE];
static volatile int write_idx = 0;
static volatile int read_idx = 0;

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void audio_buffer_init(void)
{
    write_idx = read_idx = 0;
}

void audio_buffer_write(int16_t *data, int samples)
{
    portENTER_CRITICAL(&mux);

    // Ensure stereo alignment (even number of samples)
    if (samples % 2 != 0) samples--;

    for (int i = 0; i < samples; i++)
    {
        int next = (write_idx + 1) % BUFFER_SIZE;

        if (next == read_idx)
        {
            break; // buffer full
        }

        buffer[write_idx] = data[i];
        write_idx = next;
    }

    portEXIT_CRITICAL(&mux);
}

int audio_buffer_read(int16_t *out, int max_samples)
{
    int count = 0;

    portENTER_CRITICAL(&mux);

    // Ensure stereo alignment (even number of samples)
    if (max_samples % 2 != 0) max_samples--;

    while (read_idx != write_idx && count + 1 < max_samples)
    {
        // Read stereo pair (L, R)
        out[count++] = buffer[read_idx];
        read_idx = (read_idx + 1) % BUFFER_SIZE;

        if (read_idx == write_idx || count >= max_samples) break;

        out[count++] = buffer[read_idx];
        read_idx = (read_idx + 1) % BUFFER_SIZE;
    }

    portEXIT_CRITICAL(&mux);

    return count;
}