#include "inky_display.h"

#if ENABLE_INKY_DISPLAY

#include <cstdio>

#include "drivers/uc8151/uc8151.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

using namespace pimoroni;

namespace
{
    constexpr uint16_t INKY_WIDTH = 296;
    constexpr uint16_t INKY_HEIGHT = 128;

    UC8151 g_display(INKY_WIDTH, INKY_HEIGHT, ROTATE_0);
    PicoGraphics_Pen1BitY g_graphics(INKY_WIDTH, INKY_HEIGHT, nullptr);
    bool g_ready = false;

}

bool inky_display_init(void)
{
    if (g_ready)
    {
        return true;
    }

    g_graphics.set_pen(0);
    g_graphics.clear();
    g_display.update(&g_graphics);
    g_ready = true;
    return true;
}

void inky_display_show_status(const char *wifi_ssid, const char *ip_address, bool wifi_connected)
{
    if (!inky_display_init())
    {
        return;
    }

    const char *ssid_text = (wifi_ssid && wifi_ssid[0]) ? wifi_ssid : "Wi-Fi Idle";
    const char *ip_text = (ip_address && ip_address[0])
                              ? ip_address
                              : (wifi_connected ? "Awaiting DHCP" : "Wi-Fi offline");

    // White background, black text for high contrast
    g_graphics.set_pen(15);
    g_graphics.clear();
    g_graphics.set_pen(0);
    g_graphics.set_font("bitmap8");

    g_graphics.text("Altair 8800", Point(12, 6), INKY_WIDTH - 24, 3.0f);
    g_graphics.text("pico2-w emulator", Point(12, 38), INKY_WIDTH - 24, 2.0f);

    int info_y = 78;
    char line_buffer[64];
    std::snprintf(line_buffer, sizeof(line_buffer), "SSID: %s", ssid_text);
    g_graphics.text(line_buffer, Point(12, info_y), INKY_WIDTH - 24, 1.6f);

    info_y += 20;
    std::snprintf(line_buffer, sizeof(line_buffer), "IP: %s", ip_text);
    g_graphics.text(line_buffer, Point(12, info_y), INKY_WIDTH - 24, 1.6f);

    if (!wifi_connected)
    {
        info_y += 18;
        g_graphics.text("USB console active", Point(12, info_y), INKY_WIDTH - 24, 1.4f);
    }

    g_display.update(&g_graphics);
}

#endif // ENABLE_INKY_DISPLAY
