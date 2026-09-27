#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define KNOCK_HZ_MAX 20000
#define KNOCK_MCLK_HZ 25000000u  // Y1

void knock_init(void);
bool knock_set_hz(uint32_t hz);  // 0 = off (AD9833 held in reset, VOUT at midscale)
uint32_t knock_get_hz(void);

// Hardware-independent parts, knock_codec.c. Control/register layout per the
// AD9833 datasheet, cross-checked against the Linux ad9834 driver.
#define AD9833_B28 (1u << 13)
#define AD9833_RESET (1u << 8)
#define AD9833_REG_FREQ0 (1u << 14)
#define AD9833_REG_PHASE0 (3u << 14)
#define AD9833_SEQ_MAX 5

uint32_t ad9833_freq_reg(uint32_t hz, uint32_t mclk_hz);
// Fills out[] with the 16-bit words that set the output to hz (0 = off) and
// returns how many there are, or 0 if hz is out of range.
size_t ad9833_sequence(uint32_t hz, uint32_t mclk_hz, uint16_t out[AD9833_SEQ_MAX]);
