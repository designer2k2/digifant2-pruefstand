#pragma once

#include <stdbool.h>
#include <stdint.h>

// SSD1306 128x64 on I2C (J3, 0x3C). Init sequence as in Adafruit_SSD1306
// for a 128x64 panel with its internal charge pump.
bool oled_init(void);
bool oled_flush(const uint8_t *fb);
// One 128-byte page (GFX_ROWS of them make a frame), about 3 ms on the bus instead
// of the ~23 ms a whole frame takes, so the main loop can interleave them with
// time-critical I2C traffic (the DAC waveform generator).
bool oled_flush_page(const uint8_t *fb, int page);
