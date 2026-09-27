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
