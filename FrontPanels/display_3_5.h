/*****************************************************************************
 * Waveshare 3.5" Display Front Panel Interface
 * Shows Altair 8800 title and instruction counter
 *****************************************************************************/

#ifndef __DISPLAY_3_5_H
#define __DISPLAY_3_5_H

#include <stdint.h>

/**
 * Initialize the 3.5" display
 * Must be called after SD card is initialized (they share SPI bus)
 */
void display_3_5_init(void);

/**
 * Update the instruction counter display
 * @param count Instruction count (will be displayed modulo 10000)
 */
void display_3_5_update_counter(uint32_t count);

/**
 * Deselect display SPI - call before using SD card
 */
void display_3_5_deselect(void);

#endif /* __DISPLAY_3_5_H */
