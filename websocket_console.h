#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void websocket_console_init(void);
void websocket_console_start(void);
void websocket_console_enqueue_output(uint8_t value);
bool websocket_console_try_dequeue_input(uint8_t *value);
bool websocket_console_is_running(void);
