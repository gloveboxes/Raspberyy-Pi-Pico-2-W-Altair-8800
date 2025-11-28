#pragma once

#include <stdbool.h>

#ifndef ENABLE_INKY_DISPLAY
#define ENABLE_INKY_DISPLAY 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if ENABLE_INKY_DISPLAY
bool inky_display_init(void);
void inky_display_show_status(const char *wifi_ssid, const char *ip_address, bool wifi_connected);
#else
static inline bool inky_display_init(void) { return false; }
static inline void inky_display_show_status(const char *wifi_ssid, const char *ip_address, bool wifi_connected)
{
    (void)wifi_ssid;
    (void)ip_address;
    (void)wifi_connected;
}
#endif

#ifdef __cplusplus
}
#endif
