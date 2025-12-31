/*****************************************************************************
 * Waveshare 3.5" Display Front Panel Interface
 * Shows Altair 8800 title only (simplified)
 *****************************************************************************/

#include "display_3_5.h"
#include "ili9488.h"
#include <stdio.h>

// Colors
#define BG_COLOR ILI9488_BLACK

void display_3_5_init(void)
{
    printf("[Display 3.5] Initializing...\n");

    // Initialize the ILI9488 driver
    ili9488_init();

    // Clear screen to black
    ili9488_clear(ILI9488_BLACK);

    // Draw "Altair" at position 10, 10 (top-left with margin)
    ili9488_draw_string(10, 10, "Altair", &ili9488_font16, ILI9488_WHITE, ILI9488_BLACK);

    printf("[Display 3.5] Initialized - showing 'Altair'\n");
}

void display_3_5_update_counter(uint32_t count)
{
    // Counter update disabled - display is now static
    (void)count;
}

void display_3_5_deselect(void)
{
    ili9488_deselect();
}
