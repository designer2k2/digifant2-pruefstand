#include "crank.h"

// Stub: stores the setpoint only. PIO waveform generation on GP2 comes next.

static uint32_t rpm_setpoint;

void crank_init(void) { rpm_setpoint = 0; }

bool crank_set_rpm(uint32_t rpm) {
    if (rpm > CRANK_RPM_MAX) return false;
    rpm_setpoint = rpm;
    return true;
}

uint32_t crank_get_rpm(void) { return rpm_setpoint; }

bool crank_is_implemented(void) { return false; }
