// Checks crank_compute_phases against hand-computed periods.
#include <stdio.h>
#include <stdlib.h>

#include "crank_timing.h"

static int failures;

static void expect_phases(uint32_t rpm, uint32_t ppr, uint32_t duty, uint32_t clk,
                          uint64_t want_high_cycles, uint64_t want_low_cycles) {
    crank_phases_t p;
    if (!crank_compute_phases(rpm, ppr, duty, clk, &p)) {
        printf("FAIL rpm=%u ppr=%u duty=%u: rejected\n", rpm, ppr, duty);
        failures++;
        return;
    }
    uint64_t high = (uint64_t)p.vw_high_loops + CRANK_PHASE_OVERHEAD_CYCLES;
    uint64_t low = (uint64_t)p.vw_low_loops + CRANK_PHASE_OVERHEAD_CYCLES;
    double hz = (double)clk / (double)(high + low);
    double rpm_out = hz * 60.0 / ppr;
    int ok = high == want_high_cycles && low == want_low_cycles;
    printf("%s rpm=%u ppr=%u duty=%u clk=%u: high=%llu low=%llu cycles -> %.4f Hz, %.4f rpm\n",
           ok ? "ok  " : "FAIL", rpm, ppr, duty, clk, (unsigned long long)high,
           (unsigned long long)low, hz, rpm_out);
    if (!ok) failures++;
}

static void expect_reject(uint32_t rpm, uint32_t ppr, uint32_t duty, uint32_t clk) {
    crank_phases_t p;
    int ok = !crank_compute_phases(rpm, ppr, duty, clk, &p);
    printf("%s reject rpm=%u ppr=%u duty=%u clk=%u\n", ok ? "ok  " : "FAIL", rpm, ppr, duty, clk);
    if (!ok) failures++;
}

int main(void) {
    // 850 rpm, 2 ppr at 125 MHz: 28.333 Hz, period 125e6*60/1700 = 4411764.7 -> 4411765.
    expect_phases(850, 2, 50, 125000000, 2205883, 2205882);
    // 6000 rpm, 2 ppr: 200 Hz, period exactly 625000 cycles.
    expect_phases(6000, 2, 50, 125000000, 312500, 312500);
    // Asymmetric duty, same period.
    expect_phases(6000, 2, 70, 125000000, 437500, 187500);
    // 150 MHz clock (RP2350 / overclocked RP2040): 6000 rpm -> 750000 cycles.
    expect_phases(6000, 2, 50, 150000000, 375000, 375000);
    // Slowest useful: 1 rpm, 1 ppr -> 60 s period, 7.5e9 cycles total, 3.75e9 per phase (fits 32 bit).
    expect_phases(1, 1, 50, 125000000, 3750000000ull, 3750000000ull);

    expect_reject(0, 2, 50, 125000000);
    expect_reject(850, 0, 50, 125000000);
    expect_reject(850, 2, 0, 125000000);
    expect_reject(850, 2, 100, 125000000);
    // Phase longer than a 32-bit loop count: 1 rpm, 1 ppr, 99% at 150 MHz = 8.91e9 cycles.
    expect_reject(1, 1, 99, 150000000);

    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
