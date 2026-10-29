// The generator driver (wave.c) against the fake DAC, a settable ECU power
// state and a settable injector: tick scheduling, write economy, parking,
// hand-over to a plain `dac` write, and the closed-loop mode end to end.
#include <stdio.h>
#include <string.h>

#include "capture.h"
#include "dac.h"
#include "wave.h"

extern unsigned long fake_dac_writes;
extern bool fake_inj_valid;
extern uint32_t fake_inj_low_us;

static bool ecu_on = true;
bool ecu_get_power(void) { return ecu_on; }

static int failures;

static void check(const char *what, int ok) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static uint64_t now_us;
// Advance the clock in 1 ms polls, like a busy main loop.
static void run_ms(unsigned ms) {
    for (unsigned i = 0; i < ms; i++) {
        now_us += 1000;
        wave_poll(now_us);
    }
}

static wave_cfg_t sine(uint32_t hz_milli, uint32_t amp, uint32_t off) {
    wave_cfg_t c;
    memset(&c, 0, sizeof c);
    c.shape = WAVE_SINE; c.freq_mhz = hz_milli; c.amp = amp; c.offset = off; c.duty_pct = 50;
    return c;
}

int main(void) {
    wave_poll(now_us);  // starts the schedule at t = 0

    wave_cfg_t c = sine(1000, 400, 450);
    check("a sine on the air channel starts", wave_start(DAC_AIR, &c) == NULL && wave_active(DAC_AIR));
    run_ms(251);
    uint32_t peak = dac_get_mv(DAC_AIR);
    check("a quarter period later it is at the peak (850 mV)", peak >= 848 && peak <= 852);
    run_ms(500);
    uint32_t low_mv = dac_get_mv(DAC_AIR);
    check("three quarters in it is at the lowest point (50 mV)", low_mv >= 48 && low_mv <= 52);
    check("the first tick (phase 0) was the first scheduled tick, 5 ms in", wave_t0_us(DAC_AIR) == 5000);

    // Writes only when the millivolt value changes.
    unsigned long w0 = fake_dac_writes;
    run_ms(1000);
    unsigned long per_period = fake_dac_writes - w0;
    check("at most one write per tick, fewer where the value repeats", per_period <= 200 && per_period > 100);

    // ECU off: nothing is written, but the wave keeps running.
    ecu_on = false;
    w0 = fake_dac_writes;
    run_ms(300);
    check("nothing is written while the ECU is off", fake_dac_writes == w0 && wave_active(DAC_AIR));
    ecu_on = true;
    run_ms(10);
    check("it resumes when the ECU comes back", fake_dac_writes > w0 && wave_active(DAC_AIR));

    // Someone else writes the channel: the wave steps aside.
    dac_set_mv(DAC_AIR, 1234);
    run_ms(20);
    check("a plain write ends the wave and keeps the new value",
          !wave_active(DAC_AIR) && dac_get_mv(DAC_AIR) == 1234);

    // A long stall is replayed as ticks but written once.
    wave_start(DAC_WATER, &c);
    run_ms(5);
    w0 = fake_dac_writes;
    now_us += 60000;  // one 60 ms stall
    wave_poll(now_us);
    check("a 60 ms stall costs a single write", fake_dac_writes - w0 <= 1);
    wave_stop(DAC_WATER);

    // Limits and the lambda-only shapes.
    wave_cfg_t lam = c;
    lam.shape = WAVE_LAMBDA; lam.amp = 30; lam.offset = 1000;
    check("lam is refused on a non-lambda channel", wave_start(DAC_AFM, &lam) != NULL);
    wave_cfg_t hot = sine(1000, 500, 1000);
    check("lambda channel is capped at 1200 mV", wave_max_mv(DAC_LAMBDA) == 1200 &&
                                                     wave_start(DAC_LAMBDA, &hot) != NULL);
    check("other channels keep their full range", wave_max_mv(DAC_AIR) == 3300);
    check("lam on the lambda channel starts", wave_start(DAC_LAMBDA, &lam) == NULL);
    run_ms(2000);
    uint32_t seen_min = 9999, seen_max = 0;
    for (int i = 0; i < 1000; i++) {
        run_ms(1);
        uint32_t v = dac_get_mv(DAC_LAMBDA);
        if (v < seen_min) seen_min = v;
        if (v > seen_max) seen_max = v;
    }
    check("lambda 1.00 +- 0.03 swings the sensor lean to rich", seen_min < 150 && seen_max > 750);
    wave_stop_all();
    check("wave_stop_all ends everything", !wave_active(DAC_LAMBDA) && !wave_active(DAC_AIR));

    // Closed loop, end to end: the injector time drives lambda after the delay.
    wave_cfg_t loop;
    memset(&loop, 0, sizeof loop);
    loop.shape = WAVE_LOOP; loop.offset = 1000; loop.delay_ms = 300;
    fake_inj_valid = true; fake_inj_low_us = 2600;
    dac_set_mv(DAC_LAMBDA, 0);
    check("loop starts on the lambda channel", wave_start(DAC_LAMBDA, &loop) == NULL);
    run_ms(2500);
    uint32_t neutral = dac_get_mv(DAC_LAMBDA);
    check("steady fuel keeps the sensor at 450 mV", neutral >= 440 && neutral <= 460);
    fake_inj_low_us = 2860;  // the ECU adds 10 % fuel
    run_ms(250);
    uint32_t before = dac_get_mv(DAC_LAMBDA);
    run_ms(400);
    uint32_t after = dac_get_mv(DAC_LAMBDA);
    check("it still reads neutral inside the delay", before < 480);
    check("then goes rich once the extra fuel reaches the sensor", after > 800);
    fake_inj_low_us = 2340;  // the ECU takes 10 % away
    run_ms(900);
    check("less fuel than the reference reads lean", dac_get_mv(DAC_LAMBDA) < 100);
    wave_stop_all();

    printf("%s\n", failures ? "FAILED" : "all ok");
    return failures ? 1 : 0;
}
