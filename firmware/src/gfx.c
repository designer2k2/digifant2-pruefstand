#include "gfx.h"

#include <string.h>

#include "font5x7.h"

void gfx_clear(uint8_t *fb) { memset(fb, 0, GFX_FB_SIZE); }

void gfx_text(uint8_t *fb, int col, int row, const char *s, bool invert) {
    if (row < 0 || row >= GFX_ROWS) return;
    uint8_t *line = fb + row * GFX_WIDTH;
    for (; *s && col < GFX_COLS; s++, col++) {
        if (col < 0) continue;
        char c = *s;
        if (c < FONT_FIRST || c > FONT_LAST) c = '?';
        const uint8_t *glyph = &font5x7[(c - FONT_FIRST) * FONT_WIDTH];
        int x = col * 6;
        for (int i = 0; i < FONT_WIDTH; i++) line[x + i] = invert ? (uint8_t)~glyph[i] : glyph[i];
        line[x + FONT_WIDTH] = invert ? 0xFF : 0x00;
    }
}

bool gfx_pixel(const uint8_t *fb, int x, int y) {
    return (fb[(y / 8) * GFX_WIDTH + x] >> (y % 8)) & 1;
}
