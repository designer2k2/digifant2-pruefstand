// Checks the AD9833 word sequences against hand-computed values:
// FREQREG = f * 2^28 / 25 MHz, written as FREQ0 LSB half then MSB half.
#include <stdio.h>

#include "knock.h"

static int failures;

static void expect_seq(uint32_t hz, const uint16_t *want, size_t want_n) {
    uint16_t w[AD9833_SEQ_MAX];
    size_t n = ad9833_sequence(hz, KNOCK_MCLK_HZ, w);
    int ok = n == want_n;
    for (size_t i = 0; ok && i < n; i++) ok = w[i] == want[i];
    printf("%s %5u Hz ->", ok ? "ok  " : "FAIL", hz);
    for (size_t i = 0; i < n; i++) printf(" %04X", w[i]);
    if (hz) {
        uint32_t reg = ad9833_freq_reg(hz, KNOCK_MCLK_HZ);
        double actual = (double)reg * KNOCK_MCLK_HZ / 268435456.0;
        double err = actual - hz;
        if (err < -0.05 || err > 0.05) ok = 0;
        printf("  (FREQREG %u = %.4f Hz)", reg, actual);
    }
    printf("\n");
    if (!ok) failures++;
}

int main(void) {
    const uint16_t off[] = {0x2100};
    expect_seq(0, off, 1);

    // 7000 Hz: 75161.93 -> 75162 = 0x1259A -> LSB 0x259A, MSB 0x0004.
    const uint16_t f7k[] = {0x2100, 0x659A, 0x4004, 0xC000, 0x2000};
    expect_seq(7000, f7k, 5);

    // 20000 Hz: 214748.36 -> 214748 = 0x346DC -> LSB 0x06DC, MSB 0x000D.
    const uint16_t f20k[] = {0x2100, 0x46DC, 0x400D, 0xC000, 0x2000};
    expect_seq(20000, f20k, 5);

    // 1 Hz: 10.74 -> 11.
    const uint16_t f1[] = {0x2100, 0x400B, 0x4000, 0xC000, 0x2000};
    expect_seq(1, f1, 5);

    uint16_t w[AD9833_SEQ_MAX];
    int ok = ad9833_sequence(20001, KNOCK_MCLK_HZ, w) == 0;
    printf("%s reject 20001 Hz\n", ok ? "ok  " : "FAIL");
    if (!ok) failures++;

    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
