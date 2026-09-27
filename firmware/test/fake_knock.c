// Stand-in for knock.c (which needs SPI): real range check via knock_codec.c.
#include "knock.h"

static uint32_t hz_setpoint;

void knock_init(void) {}

bool knock_set_hz(uint32_t hz) {
    uint16_t words[AD9833_SEQ_MAX];
    if (ad9833_sequence(hz, KNOCK_MCLK_HZ, words) == 0) return false;
    hz_setpoint = hz;
    return true;
}

uint32_t knock_get_hz(void) { return hz_setpoint; }
