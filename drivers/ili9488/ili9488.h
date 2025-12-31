/*****************************************************************************
 * ILI9488 LCD Driver for Waveshare 3.5" Pico Display
 * Based on Waveshare reference implementation
 *****************************************************************************/

#ifndef __ILI9488_H
#define __ILI9488_H

#include <stdbool.h>
#include <stdint.h>

// Pin definitions for Waveshare 3.5" display
#define ILI9488_RST_PIN 15
#define ILI9488_DC_PIN 8
#define ILI9488_CS_PIN 9
#define ILI9488_BKL_PIN 13
#define ILI9488_CLK_PIN 10
#define ILI9488_MOSI_PIN 11
#define ILI9488_MISO_PIN 12
#define ILI9488_TP_CS_PIN 16
#define ILI9488_SD_CS_PIN 22

// Display dimensions - matches reference L2R_U2D mode for 3.5"
// In this mode: Column=LCD_3_5_HEIGHT(320), Page=LCD_3_5_WIDTH(480)
#define ILI9488_WIDTH 320
#define ILI9488_HEIGHT 480

// RGB565 color definitions
#define ILI9488_WHITE 0xFFFF
#define ILI9488_BLACK 0x0000
#define ILI9488_BLUE 0x001F
#define ILI9488_RED 0xF800
#define ILI9488_GREEN 0x07E0
#define ILI9488_CYAN 0x07FF
#define ILI9488_MAGENTA 0xF81F
#define ILI9488_YELLOW 0xFFE0
#define ILI9488_GRAY 0x8430

// Font structure
typedef struct
{
    const uint8_t* table;
    uint16_t Width;
    uint16_t Height;
} ili9488_font_t;

// External font declarations
extern ili9488_font_t ili9488_font16;
extern ili9488_font_t ili9488_font24;

/**
 * Initialize the ILI9488 display
 * Sets up SPI, GPIO pins, and sends initialization commands
 */
void ili9488_init(void);

/**
 * Clear the entire display with a color
 */
void ili9488_clear(uint16_t color);

/**
 * Draw a single character
 */
void ili9488_draw_char(uint16_t x, uint16_t y, char c, ili9488_font_t* font, uint16_t fg_color, uint16_t bg_color);

/**
 * Draw a string
 */
void ili9488_draw_string(uint16_t x, uint16_t y, const char* str, ili9488_font_t* font, uint16_t fg_color,
                         uint16_t bg_color);

/**
 * Draw a number
 */
void ili9488_draw_number(uint16_t x, uint16_t y, int32_t number, ili9488_font_t* font, uint16_t fg_color,
                         uint16_t bg_color);

/**
 * Fill a rectangular area with a color
 */
void ili9488_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * Deselect the display (set CS high) - call before using other SPI devices
 */
void ili9488_deselect(void);

#endif /* __ILI9488_H */
