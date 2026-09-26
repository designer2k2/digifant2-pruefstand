#pragma once

#include <stdbool.h>
#include <stdint.h>

// Fixed PIO overhead per phase, see crank.pio.
#define CRANK_PHASE_OVERHEAD_CYCLES 4

typedef struct {
    uint32_t vw_low_loops;   // PIO loop count for the VW-18-low phase (GP2 high)
    uint32_t vw_high_loops;  // PIO loop count for the VW-18-high phase (GP2 low)
} crank_phases_t;

// Converts crank RPM, pulses per crank revolution and duty (percent of the
// period VW-18 is HIGH) into PIO loop counts for a state machine clocked at
// clk_hz. Returns false if a phase would be shorter than the PIO overhead or
// longer than a 32-bit count.
bool crank_compute_phases(uint32_t rpm, uint32_t ppr, uint32_t duty_pct,
                          uint32_t clk_hz, crank_phases_t *out);
