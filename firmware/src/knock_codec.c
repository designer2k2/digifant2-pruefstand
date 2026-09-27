#include "knock.h"

uint32_t ad9833_freq_reg(uint32_t hz, uint32_t mclk_hz) {
    return (uint32_t)((((uint64_t)hz << 28) + mclk_hz / 2) / mclk_hz);
}

size_t ad9833_sequence(uint32_t hz, uint32_t mclk_hz, uint16_t out[AD9833_SEQ_MAX]) {
    if (hz > KNOCK_HZ_MAX) return 0;
    if (hz == 0) {
        out[0] = AD9833_B28 | AD9833_RESET;
        return 1;
    }
    // Hold in reset while loading FREQ0 as two 14-bit halves (B28 = 1, LSB half
    // first) and zeroing PHASE0, then release reset: sine output from phase 0.
    uint32_t reg = ad9833_freq_reg(hz, mclk_hz);
    out[0] = AD9833_B28 | AD9833_RESET;
    out[1] = (uint16_t)(AD9833_REG_FREQ0 | (reg & 0x3FFF));
    out[2] = (uint16_t)(AD9833_REG_FREQ0 | ((reg >> 14) & 0x3FFF));
    out[3] = AD9833_REG_PHASE0;
    out[4] = AD9833_B28;
    return 5;
}

bool knock_burst_valid(uint32_t start_deg, uint32_t len_deg, uint32_t every, uint32_t ppr) {
    if (ppr == 0 || len_deg == 0 || every == 0) return false;
    return ((uint64_t)start_deg + len_deg) * ppr < 360;
}

bool knock_burst_timing(const knock_burst_t *b, uint32_t ref_index, uint32_t ref_period_us,
                        uint32_t ppr, uint32_t *start_us, uint32_t *len_us) {
    if (!b->enabled || ref_period_us == 0) return false;
    if (!knock_burst_valid(b->start_deg, b->len_deg, b->every, ppr)) return false;
    if (ref_index % b->every != 0) return false;
    // One reference period spans 360/ppr crank degrees.
    *start_us = (uint32_t)((uint64_t)ref_period_us * b->start_deg * ppr / 360u);
    *len_us = (uint32_t)((uint64_t)ref_period_us * b->len_deg * ppr / 360u);
    return true;
}
