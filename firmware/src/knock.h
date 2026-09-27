#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define KNOCK_HZ_MAX 20000
#define KNOCK_MCLK_HZ 25000000u  // Y1

// Output: off if hz is 0; otherwise a continuous tone, or bursts timed to the
// crank reference (VW-18 falling edge) while burst mode is on.
typedef struct {
    bool enabled;
    uint32_t start_deg;  // crank degrees after the reference
    uint32_t len_deg;
    uint32_t every;      // burst on every Nth reference (1 = all)
} knock_burst_t;

void knock_init(void);
bool knock_set_hz(uint32_t hz);  // 0 = off (AD9833 held in reset, VOUT at midscale)
uint32_t knock_get_hz(void);
bool knock_set_burst(uint32_t start_deg, uint32_t len_deg, uint32_t every);
void knock_burst_off(void);
knock_burst_t knock_get_burst(void);

// Called from the capture interrupt on every crank reference edge.
void knock_on_crank_ref(uint32_t now_us, uint32_t ref_period_us);

// Hardware-independent parts, knock_codec.c. Control/register layout per the
// AD9833 datasheet, cross-checked against the Linux ad9834 driver.
#define AD9833_B28 (1u << 13)
#define AD9833_RESET (1u << 8)
#define AD9833_REG_FREQ0 (1u << 14)
#define AD9833_REG_PHASE0 (3u << 14)
#define AD9833_SEQ_MAX 5

uint32_t ad9833_freq_reg(uint32_t hz, uint32_t mclk_hz);
// Fills out[] with the 16-bit words that set the output to hz (0 = off) and
// returns how many there are, or 0 if hz is out of range. For hz > 0 the last
// word releases reset; leave it off to load a frequency without starting it.
size_t ad9833_sequence(uint32_t hz, uint32_t mclk_hz, uint16_t out[AD9833_SEQ_MAX]);

// A burst must start and end within one reference period (360/ppr degrees),
// so it can't still be running when the next reference arrives.
bool knock_burst_valid(uint32_t start_deg, uint32_t len_deg, uint32_t every, uint32_t ppr);
// For the ref_index-th reference: whether it gets a burst, and when it starts
// and how long it lasts in microseconds.
bool knock_burst_timing(const knock_burst_t *b, uint32_t ref_index, uint32_t ref_period_us,
                        uint32_t ppr, uint32_t *start_us, uint32_t *len_us);
