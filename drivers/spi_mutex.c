/*****************************************************************************
 * SPI Bus Mutex for shared SPI1 access
 * Used by SD card and ILI9488 display on Waveshare 3.5" board
 *****************************************************************************/

#include "spi_mutex.h"
#include "pico/mutex.h"

static mutex_t spi_bus_mutex;
static bool mutex_initialized = false;

void spi_mutex_init(void)
{
    if (!mutex_initialized)
    {
        mutex_init(&spi_bus_mutex);
        mutex_initialized = true;
    }
}

void spi_mutex_enter(void)
{
    if (mutex_initialized)
    {
        mutex_enter_blocking(&spi_bus_mutex);
    }
}

void spi_mutex_exit(void)
{
    if (mutex_initialized)
    {
        mutex_exit(&spi_bus_mutex);
    }
}
