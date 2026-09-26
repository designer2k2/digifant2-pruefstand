#include "knock.h"

// Stub: stores the setpoint only. AD9833 SPI control comes later.

static uint32_t hz_setpoint;

void knock_init(void) { hz_setpoint = 0; }

bool knock_set_hz(uint32_t hz) {
    if (hz > KNOCK_HZ_MAX) return false;
    hz_setpoint = hz;
    return true;
}

uint32_t knock_get_hz(void) { return hz_setpoint; }

bool knock_is_implemented(void) { return false; }
