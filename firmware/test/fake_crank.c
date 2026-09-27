// Stand-in for crank.c (which needs the PIO): same validation, no waveform.
#include "crank.h"
#include "crank_timing.h"

static uint32_t rpm, ppr = CRANK_PPR_DEFAULT, duty = CRANK_DUTY_DEFAULT;

static bool valid(uint32_t r, uint32_t p, uint32_t d) {
    crank_phases_t unused;
    return r == 0 || crank_compute_phases(r, p, d, 125000000, &unused);
}

void crank_init(void) {}

bool crank_set_rpm(uint32_t r) {
    if (r > CRANK_RPM_MAX || !valid(r, ppr, duty)) return false;
    rpm = r;
    return true;
}
uint32_t crank_get_rpm(void) { return rpm; }

bool crank_set_ppr(uint32_t p) {
    if (p == 0 || p > CRANK_PPR_MAX || !valid(rpm, p, duty)) return false;
    ppr = p;
    return true;
}
uint32_t crank_get_ppr(void) { return ppr; }

bool crank_set_duty(uint32_t d) {
    if (d == 0 || d >= 100 || !valid(rpm, ppr, d)) return false;
    duty = d;
    return true;
}
uint32_t crank_get_duty(void) { return duty; }

