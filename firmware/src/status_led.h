#pragma once

#include <stdint.h>

// D2 (SK6812) on GP10, driven by pio1; see status_led_core.h for the colours.
void status_led_init(void);
void status_led_poll(uint32_t now_ms);  // main loop
