// Checks MCP4728 Multi-Write frames against hand-built bytes (DS22187E fig. 5-8)
// and decodes each back to a voltage with the datasheet's equation 4-1.
#include <stdio.h>

#include "dac.h"

static int failures;

static void expect_frame(dac_channel_t ch, uint32_t mv, uint8_t b0, uint8_t b1, uint8_t b2) {
    uint8_t f[3];
    if (!dac_encode(ch, mv, f)) {
        printf("FAIL %s %u mV rejected\n", dac_channel_name(ch), mv);
        failures++;
        return;
    }
    // Decode: channel = bits 2:1 of byte 0, VREF bit 7 / gain bit 4 of byte 1.
    unsigned channel = (f[0] >> 1) & 3;
    unsigned vref_internal = f[1] >> 7;
    unsigned gain = (f[1] >> 4) & 1 ? 2 : 1;
    unsigned code = ((f[1] & 0x0F) << 8) | f[2];
    double vout = 2.048 * code / 4096.0 * gain;

    int ok = f[0] == b0 && f[1] == b1 && f[2] == b2 && channel == (unsigned)ch &&
             vref_internal && (f[0] & 0xE1) == 0x40 && ((f[1] >> 5) & 3) == 0 &&
             vout * 1000.0 > mv - 0.001 && vout * 1000.0 < mv + 0.001;
    printf("%s %-6s %4u mV -> %02X %02X %02X (ch %u, gain %u, code %u, %.4f V)\n",
           ok ? "ok  " : "FAIL", dac_channel_name(ch), mv, f[0], f[1], f[2], channel, gain,
           code, vout);
    if (!ok) failures++;
}

static void expect_reject(dac_channel_t ch, uint32_t mv) {
    uint8_t f[3];
    int ok = !dac_encode(ch, mv, f);
    printf("%s reject %s %u mV\n", ok ? "ok  " : "FAIL", dac_channel_name(ch), mv);
    if (!ok) failures++;
}

int main(void) {
    expect_frame(DAC_AIR, 0, 0x40, 0x90, 0x00);
    expect_frame(DAC_AIR, 1234, 0x40, 0x94, 0xD2);      // code 0x4D2
    expect_frame(DAC_WATER, 3300, 0x42, 0x9C, 0xE4);    // code 0xCE4
    expect_frame(DAC_AFM, 1, 0x44, 0x90, 0x01);
    expect_frame(DAC_LAMBDA, 450, 0x46, 0x83, 0x84);    // gain 1: code 900 = 0x384
    expect_frame(DAC_LAMBDA, 2047, 0x46, 0x8F, 0xFE);   // code 4094

    expect_reject(DAC_AIR, 3301);
    expect_reject(DAC_LAMBDA, 2048);

    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
