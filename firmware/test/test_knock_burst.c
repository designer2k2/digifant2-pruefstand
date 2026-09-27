// Runs the real knock.c against the simulated SDK in test/fake_sdk: feeds it
// crank references on a simulated clock and checks exactly which AD9833 words
// go out, and when. Also covers the burst angle/length math in knock_codec.c.
#include <stdio.h>
#include <string.h>

#include "crank.h"
#include "knock.h"
#include "sim.h"

static uint32_t ppr = 2;
uint32_t crank_get_ppr(void) { return ppr; }

static int failures;

static void check(const char *what, int ok) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

// Runs any alarm due up to t, then sets the clock to t.
static void advance_to(uint64_t t) {
    while (sim_alarm_armed && sim_alarm_target <= t) {
        sim_now_us = sim_alarm_target;
        sim_alarm_armed = false;
        sim_alarm_cb(0);
    }
    sim_now_us = t;
}

static void ref_at(uint64_t t, uint32_t period_us) {
    advance_to(t);
    knock_on_crank_ref((uint32_t)t, period_us);
}

static void clear_log(void) { sim_log_n = 0; }

static int log_is(const uint16_t *words, const uint64_t *times, size_t n) {
    if (sim_log_n != n) return 0;
    for (size_t i = 0; i < n; i++) {
        if (sim_log_word[i] != words[i] || sim_log_t[i] != times[i]) return 0;
    }
    return 1;
}

static void dump_log(void) {
    for (size_t i = 0; i < sim_log_n; i++) {
        printf("      t=%llu %04X\n", (unsigned long long)sim_log_t[i], sim_log_word[i]);
    }
}

int main(void) {
    // --- timing math ---
    knock_burst_t b = {true, 30, 20, 1};
    uint32_t s, l;
    check("850 rpm, 2 ppr: 30 deg -> 5882 us, 20 deg -> 3921 us",
          knock_burst_timing(&b, 0, 35294, 2, &s, &l) && s == 5882 && l == 3921);
    check("start+len must stay inside one reference (180 deg at 2 ppr)",
          knock_burst_valid(100, 79, 1, 2) && !knock_burst_valid(100, 80, 1, 2));
    check("7 ppr (51.4 deg per reference): 10+41 ok, 10+42 rejected",
          knock_burst_valid(10, 41, 1, 7) && !knock_burst_valid(10, 42, 1, 7));
    check("zero length / every / ppr rejected",
          !knock_burst_valid(10, 0, 1, 2) && !knock_burst_valid(10, 5, 0, 2) &&
              !knock_burst_valid(10, 5, 1, 0));
    knock_burst_t b3 = {true, 30, 20, 3};
    check("every 3: bursts on references 0 and 3 only",
          knock_burst_timing(&b3, 0, 35294, 2, &s, &l) && !knock_burst_timing(&b3, 1, 35294, 2, &s, &l) &&
              !knock_burst_timing(&b3, 2, 35294, 2, &s, &l) && knock_burst_timing(&b3, 3, 35294, 2, &s, &l));

    // --- state machine on the simulated clock ---
    const uint32_t P = 35294;  // 850 rpm at 2 ppr
    sim_now_us = 1000000;
    knock_init();
    knock_set_hz(7000);
    clear_log();

    knock_set_burst(30, 20, 1);
    const uint16_t loaded[] = {0x2100, 0x659A, 0x4004, 0xC000};
    const uint64_t loaded_t[] = {1000000, 1000000, 1000000, 1000000};
    check("entering burst mode loads 7000 Hz but stays in reset", log_is(loaded, loaded_t, 4));

    clear_log();
    uint64_t r0 = 2000000;
    for (int i = 0; i < 3; i++) ref_at(r0 + (uint64_t)i * P, P);
    advance_to(r0 + 3 * (uint64_t)P);
    const uint16_t w3[] = {0x2000, 0x2100, 0x2000, 0x2100, 0x2000, 0x2100};
    const uint64_t t3[] = {r0 + 5882, r0 + 5882 + 3921,
                           r0 + P + 5882, r0 + P + 5882 + 3921,
                           r0 + 2 * P + 5882, r0 + 2 * P + 5882 + 3921};
    int ok = log_is(w3, t3, 6);
    check("each reference: RUN at +30 deg (5882 us), RESET 20 deg later (3921 us)", ok);
    if (!ok) dump_log();

    // every 2: only references 0 and 2 of the new sequence.
    knock_set_burst(30, 20, 2);
    clear_log();
    uint64_t r1 = r0 + 10 * (uint64_t)P;
    for (int i = 0; i < 4; i++) ref_at(r1 + (uint64_t)i * P, P);
    advance_to(r1 + 4 * (uint64_t)P);
    const uint16_t w2[] = {0x2000, 0x2100, 0x2000, 0x2100};
    const uint64_t t2[] = {r1 + 5882, r1 + 5882 + 3921, r1 + 2 * P + 5882, r1 + 2 * P + 5882 + 3921};
    ok = log_is(w2, t2, 4);
    check("every 2: bursts on alternate references only", ok);
    if (!ok) dump_log();

    // Start at 0 deg: the start time has already passed when the reference is
    // handled, so the burst starts immediately instead of being lost.
    knock_set_burst(0, 20, 1);
    clear_log();
    uint64_t r2 = r1 + 10 * (uint64_t)P;
    ref_at(r2, P);
    advance_to(r2 + P / 2);
    const uint16_t w0[] = {0x2000, 0x2100};
    const uint64_t t0[] = {r2, r2 + 3921};
    ok = log_is(w0, t0, 2);
    check("start at 0 deg: missed alarm handled inline, burst still runs", ok);
    if (!ok) dump_log();

    // RPM jumps: the next reference arrives while a burst is still on. It must
    // be cut off at that reference, and the next burst uses the new period.
    knock_set_burst(100, 70, 1);  // at P: starts 19607 us (truncated), runs 13725 us
    clear_log();
    uint64_t r3 = r2 + 10 * (uint64_t)P;
    ref_at(r3, P);                 // RUN at r3+19607, stop due at r3+33332
    uint32_t P2 = 25000;           // next reference comes early
    ref_at(r3 + P2, P2);           // tone is on at this point
    advance_to(r3 + P2 + P2);
    // P2 bursts: start 100/180*25000 = 13888, len 70/180*25000 = 9722.
    const uint16_t wj[] = {0x2000, 0x2100, 0x2000, 0x2100};
    const uint64_t tj[] = {r3 + 19607, r3 + P2, r3 + P2 + 13888, r3 + P2 + 13888 + 9722};
    ok = log_is(wj, tj, 4);
    check("burst still on at the next reference is cut off there, next one rescheduled", ok);
    if (!ok) dump_log();

    // Burst off returns to a continuous tone; no more words at references.
    knock_burst_off();
    const uint16_t cont_tail = 0x2000;
    check("burst off: full sequence ending in RUN (continuous tone)",
          sim_log_n >= 5 && sim_log_word[sim_log_n - 1] == cont_tail);
    clear_log();
    uint64_t r4 = r3 + 10 * (uint64_t)P;
    ref_at(r4, P);
    advance_to(r4 + P);
    check("burst off: references no longer touch the chip", sim_log_n == 0 && !sim_alarm_armed);

    // knock off during burst mode: reset, and references stay silent.
    knock_set_burst(30, 20, 1);
    knock_set_hz(0);
    clear_log();
    ref_at(r4 + 2 * (uint64_t)P, P);
    advance_to(r4 + 3 * (uint64_t)P);
    check("knock off in burst mode: no bursts", sim_log_n == 0 && !sim_alarm_armed);

    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
