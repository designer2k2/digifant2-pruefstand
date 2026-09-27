#include "oled.h"

#include "gfx.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pins.h"

#define CTRL_COMMANDS 0x00
#define CTRL_DATA 0x40
#define TIMEOUT_US 50000

static bool commands(const uint8_t *cmds, size_t n) {
    uint8_t buf[32];
    buf[0] = CTRL_COMMANDS;
    for (size_t i = 0; i < n; i++) buf[i + 1] = cmds[i];
    return i2c_write_timeout_us(I2C_PORT, I2C_ADDR_OLED, buf, n + 1, false, TIMEOUT_US) ==
           (int)(n + 1);
}

bool oled_init(void) {
    static const uint8_t init[] = {
        0xAE,        // display off
        0xD5, 0x80,  // clock divide ratio
        0xA8, 0x3F,  // multiplex: 64 rows
        0xD3, 0x00,  // display offset 0
        0x40,        // start line 0
        0x8D, 0x14,  // charge pump on
        0x20, 0x00,  // horizontal addressing
        0xA1,        // segment remap
        0xC8,        // COM scan descending
        0xDA, 0x12,  // COM pins, 128x64
        0x81, 0xCF,  // contrast
        0xD9, 0xF1,  // pre-charge
        0xDB, 0x40,  // VCOMH deselect level
        0xA4,        // display follows RAM
        0xA6,        // normal (not inverted)
        0x2E,        // scrolling off
        0xAF,        // display on
    };
    return commands(init, sizeof init);
}

bool oled_flush(const uint8_t *fb) {
    static const uint8_t window[] = {0x21, 0, GFX_WIDTH - 1, 0x22, 0, GFX_ROWS - 1};
    if (!commands(window, sizeof window)) return false;
    static uint8_t buf[GFX_FB_SIZE + 1];
    buf[0] = CTRL_DATA;
    for (int i = 0; i < GFX_FB_SIZE; i++) buf[i + 1] = fb[i];
    return i2c_write_timeout_us(I2C_PORT, I2C_ADDR_OLED, buf, sizeof buf, false, TIMEOUT_US) ==
           (int)sizeof buf;
}
