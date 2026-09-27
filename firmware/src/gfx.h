#pragma once

#include <stdbool.h>
#include <stdint.h>

// 128x64 monochrome framebuffer in SSD1306 page order: byte (page * 128 + x)
// holds 8 vertical pixels of column x, LSB at the top.
#define GFX_WIDTH 128
#define GFX_HEIGHT 64
#define GFX_FB_SIZE (GFX_WIDTH * GFX_HEIGHT / 8)

// Text grid: 6-pixel-wide cells (5x7 glyph + 1 blank column), one per page.
#define GFX_COLS (GFX_WIDTH / 6)  // 21
#define GFX_ROWS (GFX_HEIGHT / 8) // 8

void gfx_clear(uint8_t *fb);
// Draws text at a text-grid cell, clipped at the right edge. Inverted text
// also inverts the gap column after each glyph, so a field reads as one bar.
void gfx_text(uint8_t *fb, int col, int row, const char *s, bool invert);
bool gfx_pixel(const uint8_t *fb, int x, int y);
