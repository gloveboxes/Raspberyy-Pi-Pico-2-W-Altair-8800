/*****************************************************************************
 * SPI Bus Mutex for shared SPI1 access
 * Used by SD card and ILI9488 display on Waveshare 3.5" board
 *****************************************************************************/

#ifndef __SPI_MUTEX_H
#define __SPI_MUTEX_H

#include "pico/mutex.h"

// Initialize the SPI mutex - call once at startup before any SPI use
void spi_mutex_init(void);

// Acquire exclusive access to SPI bus
void spi_mutex_enter(void);

// Release SPI bus access
void spi_mutex_exit(void);

#endif /* __SPI_MUTEX_H */
