#include "PortDrivers/utility_io.h"

#include "pico/time.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "build_version.h"

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
    size_t len = 0;

    switch (port)
    {
    case 45:
        if (buffer != NULL && buffer_length >= 2)
        {
            ensure_rng_seeded();
            uint16_t value = (uint16_t)(rand() & 0xFFFF);
            buffer[0] = (char)(value & 0x00FF);
            buffer[1] = (char)((value >> 8) & 0x00FF);
            len = 2;
            break;
        }
    case 70: // Load Altair version number
        if (buffer != NULL && buffer_length > 0)
        {
            len = (size_t)snprintf(buffer, buffer_length, "%d (%s %s)\n", BUILD_VERSION, BUILD_DATE, BUILD_TIME);
        }
        break;
    default:
        return 0;
    }

    return len;
}

uint8_t utility_input(uint8_t port)
{
    (void)port;
    return 0;
}
