// Waveform maths (wave_core.c): shapes, ramp, the sensor curve and its lag,
// validation, and the closed-loop lambda emulation.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "wave_core.h"

static int failures;

static void check(const char *what, int ok) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static int near(double a, double b, double tol) { return fabs(a - b) <= tol; }

static wave_cfg_t cfg(wave_shape_t s) {
    wave_cfg_t c;
    memset(&c, 0, sizeof c);
    c.shape = s;
    c.duty_pct = 50;
    return c;
}

int main(void) {
    // ---- phase and unit shapes
    uint32_t q = 0;
    for (int i = 0; i < 5; i++) q += wave_phase_step(1000, 50000);  // 1 Hz for 250 ms
    check("1 Hz for 250 ms is a quarter cycle", q > 0x3FFFFFF0u && q < 0x40000010u);
    check("a 10 s stall is clamped to 100 ms of phase",
          wave_phase_step(1000, 10000000) == wave_phase_step(1000, 100000));
    check("sine: 0, +1, -1", near(wave_unit(WAVE_SINE, 0, 0), 0, 1e-6) &&
                                 near(wave_unit(WAVE_SINE, 0x40000000u, 0), 1, 1e-5) &&
                                 near(wave_unit(WAVE_SINE, 0xC0000000u, 0), -1, 1e-5));
    check("triangle: 0, +1, 0, -1", near(wave_unit(WAVE_TRIANGLE, 0, 0), 0, 1e-6) &&
                                        near(wave_unit(WAVE_TRIANGLE, 0x40000000u, 0), 1, 1e-5) &&
                                        near(wave_unit(WAVE_TRIANGLE, 0x80000000u, 0), 0, 1e-5) &&
                                        near(wave_unit(WAVE_TRIANGLE, 0xC0000000u, 0), -1, 1e-5));
    check("square 25 %: high for the first quarter only",
          wave_unit(WAVE_SQUARE, 0x20000000u, 25) == 1.0f &&
              wave_unit(WAVE_SQUARE, 0x50000000u, 25) == -1.0f);

    // ---- stepping a sine through one period
    wave_cfg_t c = cfg(WAVE_SINE);
    c.freq_mhz = 1000; c.amp = 400; c.offset = 450;
    wave_state_t s;
    wave_state_reset(&s);
    uint32_t mn = 99999, mx = 0, at50 = 0;
    for (int i = 0; i < 200; i++) {
        uint32_t v = wave_step(&s, &c, 3300);
        if (i == 50) at50 = v;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    check("sine 450 +- 400 mV: swings 50..850", mn >= 49 && mn <= 51 && mx >= 849 && mx <= 851);
    check("quarter period (tick 50) is the peak", at50 >= 849 && at50 <= 851);
    wave_state_reset(&s);
    wave_cfg_t big = c; big.amp = 440; big.offset = 450;
    uint32_t cl = 0;
    for (int i = 0; i < 200; i++) { uint32_t v = wave_step(&s, &big, 600); if (v > cl) cl = v; }
    check("output is clamped to the channel maximum", cl == 600);

    // ---- ramp
    wave_cfg_t r = cfg(WAVE_RAMP);
    r.from_mv = 0; r.to_mv = 1000; r.ramp_ms = 2000;
    wave_state_reset(&s);
    uint32_t v0 = wave_step(&s, &r, 3300), vmid = 0, vend = 0;
    for (int i = 1; i <= 500; i++) {
        uint32_t v = wave_step(&s, &r, 3300);
        if (i == 200) vmid = v;
        if (i == 500) vend = v;
    }
    check("ramp starts at 'from', is half way at half time, then holds 'to'",
          v0 == 0 && vmid >= 499 && vmid <= 501 && vend == 1000);

    // ---- sensor curve and lag
    check("sensor: 450 mV at lambda 1", near(wave_sensor_mv(1.0f), 450, 1));
    check("sensor: rich at 0.97, lean at 1.03",
          wave_sensor_mv(0.97f) > 800 && wave_sensor_mv(1.03f) < 100);
    int mono = 1;
    for (float l = 0.90f; l < 1.10f; l += 0.01f) mono &= wave_sensor_mv(l) >= wave_sensor_mv(l + 0.01f);
    check("sensor curve falls monotonically with lambda", mono);
    float y = 0;
    for (int i = 0; i < 20; i++) y = wave_lag(y, 100.0f, WAVE_TICK_US, 100);
    check("lag reaches ~63 % after one time constant", y > 60 && y < 65);

    // ---- lambda shape
    wave_cfg_t lam = cfg(WAVE_LAMBDA);
    lam.freq_mhz = 1000; lam.amp = 30; lam.offset = 1000;
    wave_state_reset(&s);
    mn = 99999; mx = 0;
    double sum = 0;
    for (int i = 0; i < 400; i++) {
        uint32_t v = wave_step(&s, &lam, 2047);
        if (i >= 200) { sum += v; if (v < mn) mn = v; if (v > mx) mx = v; }
    }
    check("lambda 1.00 +- 0.03 at 1 Hz switches the sensor between lean and rich",
          mn < 150 && mx > 750 && mx <= WAVE_LAMBDA_MAX_MV);
    check("... and averages about 450 mV", near(sum / 200, 450, 60));
    lam.amp = 0;
    wave_state_reset(&s);
    uint32_t flat = 0;
    for (int i = 0; i < 50; i++) flat = wave_step(&s, &lam, 2047);
    check("lambda amplitude 0 holds 450 mV", flat >= 449 && flat <= 451);

    // ---- validation
    wave_cfg_t t = c;
    check("valid sine accepted", wave_validate(&t, 3300) == NULL);
    t = c; t.freq_mhz = 40;
    check("0.04 Hz refused", wave_validate(&t, 3300) != NULL);
    t = c; t.freq_mhz = 10001;
    check("10.001 Hz refused", wave_validate(&t, 3300) != NULL);
    t = c; t.amp = 500;
    check("offset below amp refused", wave_validate(&t, 3300) != NULL);
    t = c; t.offset = 3000;
    check("offset + amp above the channel maximum refused", wave_validate(&t, 3300) != NULL);
    t = cfg(WAVE_SQUARE); t.freq_mhz = 1000; t.amp = 100; t.offset = 500; t.duty_pct = 0;
    check("square duty 0 refused", wave_validate(&t, 3300) != NULL);
    t = cfg(WAVE_LAMBDA); t.freq_mhz = 1000; t.amp = 600; t.offset = 1000;
    check("lambda amplitude above 0.500 refused", wave_validate(&t, 1200) != NULL);
    t = cfg(WAVE_LOOP); t.offset = 1000; t.delay_ms = 2000;
    check("loop delay above 1500 ms refused", wave_validate(&t, 1200) != NULL);
    t = cfg(WAVE_RAMP); t.from_mv = 0; t.to_mv = 100; t.ramp_ms = 50;
    check("ramp shorter than 0.1 s refused", wave_validate(&t, 3300) != NULL);

    // ---- closed-loop emulation
    wave_cfg_t lp = cfg(WAVE_LOOP);
    lp.offset = 1000;  // lambda0 = 1.000
    lp.delay_ms = 500;
    wave_loop_t l;
    wave_loop_reset(&l);
    uint32_t out = 0;
    for (int i = 0; i < 300; i++) out = wave_loop_step(&l, &lp, true, 2600);
    check("steady injector time at lambda0 = 1.0 holds 450 mV", out >= 445 && out <= 455);

    int first_rich = -1;
    for (int i = 0; i < 200; i++) {  // injector time +10 % from tick 0 of this loop
        out = wave_loop_step(&l, &lp, true, 2860);
        if (first_rich < 0 && out > 520) first_rich = i;
    }
    check("+10 % fuel drives the sensor rich", out > 800);
    check("... but only after the 500 ms transport delay (100 ticks)",
          first_rich >= 100 && first_rich <= 110);

    wave_loop_reset(&l);
    lp.offset = 1050;  // starts lean
    for (int i = 0; i < 300; i++) out = wave_loop_step(&l, &lp, true, 2600);
    check("lambda0 1.05 reads lean (sensor near 0)", out < 20);

    wave_loop_reset(&l);
    lp.offset = 1000;
    lp.ref_us = 3000;  // given, not learned: effective at once
    for (int i = 0; i < 100; i++) out = wave_loop_step(&l, &lp, true, 3000);
    check("explicit reference works without the learning second", out >= 445 && out <= 455);

    wave_loop_reset(&l);
    lp.ref_us = 0;
    for (int i = 0; i < 300; i++) out = wave_loop_step(&l, &lp, false, 0);
    check("no injector pulses: stays neutral (450 mV)", out >= 445 && out <= 455);

    printf("%s\n", failures ? "FAILED" : "all ok");
    return failures ? 1 : 0;
}
