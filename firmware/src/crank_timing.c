#include "crank_timing.h"

static bool cycles_to_loops(uint64_t cycles, uint32_t *loops) {
    if (cycles < CRANK_PHASE_OVERHEAD_CYCLES) return false;
    uint64_t n = cycles - CRANK_PHASE_OVERHEAD_CYCLES;
    if (n > UINT32_MAX) return false;
    *loops = (uint32_t)n;
    return true;
}

bool crank_compute_phases(uint32_t rpm, uint32_t ppr, uint32_t duty_pct,
                          uint32_t clk_hz, crank_phases_t *out) {
    if (rpm == 0 || ppr == 0 || duty_pct == 0 || duty_pct >= 100) return false;

    // period = 60 s / (rpm * ppr), in clock cycles, rounded to nearest.
    uint64_t pulses_per_min = (uint64_t)rpm * ppr;
    uint64_t period = ((uint64_t)clk_hz * 60u + pulses_per_min / 2) / pulses_per_min;
    uint64_t high = (period * duty_pct + 50) / 100;
    uint64_t low = period - high;

    return cycles_to_loops(high, &out->vw_high_loops) &&
           cycles_to_loops(low, &out->vw_low_loops);
}
