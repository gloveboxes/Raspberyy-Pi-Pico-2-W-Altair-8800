#include "PortDrivers/utility_io.h"

#include "pico/time.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static bool rng_seeded = false;

static void ensure_rng_seeded(void)
{
    if (!rng_seeded)
    {
        uint64_t seed = time_us_64() ^ (uint64_t)(uintptr_t)&rng_seeded;
        srand((unsigned int)seed);
        rng_seeded = true;
    }
}

size_t utility_output(int port, uint8_t data, char *buffer, size_t buffer_length)
{
    (void)data;

    switch (port)
    {
        case 45:
            if (buffer != NULL && buffer_length >= 2)
            {
                ensure_rng_seeded();
                uint16_t value = (uint16_t)(rand() & 0xFFFF);
                buffer[0]      = (char)(value & 0x00FF);
                buffer[1]      = (char)((value >> 8) & 0x00FF);
                return 2;
            }
            return 0;
        default:
            return 0;
    }
}

uint8_t utility_input(uint8_t port)
{
    (void)port;
    return 0;
}
