#pragma once

#include <stdbool.h>
#include <stdint.h>

// SSD1306 128x64 on I2C (J3, 0x3C). Init sequence as in Adafruit_SSD1306
// for a 128x64 panel with its internal charge pump.
bool oled_init(void);
bool oled_flush(const uint8_t *fb);
