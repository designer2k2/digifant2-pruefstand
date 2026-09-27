// Drives the capture bookkeeping with a simulated engine and checks the
// periods, pulse widths and crank angles it reports. Timestamps start just
// below 2^32 us (71.6 min, where a 32-bit microsecond clock would wrap).
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "capture.h"

static int failures;

static void check(const char *what, int ok) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static int near(float a, float b, float tol) { return fabsf(a - b) <= tol; }

#define T0 (0xFFFFFFFFull - 50000u)

int main(void) {
    const uint32_t ppr = 2;
    const double ref_us = 60e6 / (850.0 * ppr);  // 850 rpm: 35294 us per reference
    const double us_per_deg = ref_us / (360.0 / ppr);
    cap_state_t s;
    memset(&s, 0, sizeof s);

    // Ignition once per reference: falls at 40 deg, rises at 90 deg.
    // Injector once per two references: falls at 10 deg, 2400 us wide.
    uint64_t t = T0;
    for (int i = 0; i < 6; i++) {
        uint64_t ref = t + (uint64_t)llround(i * ref_us);
        cap_on_ref(&s, ref);
        cap_on_edge(&s, CAP_IGN, false, ref + (uint64_t)llround(40 * us_per_deg));
        cap_on_edge(&s, CAP_IGN, true, ref + (uint64_t)llround(90 * us_per_deg));
        if (i % 2 == 0) {
            cap_on_edge(&s, CAP_INJ, false, ref + (uint64_t)llround(10 * us_per_deg));
            cap_on_edge(&s, CAP_INJ, true, ref + (uint64_t)llround(10 * us_per_deg) + 2400);
        }
    }
    uint64_t now = T0 + (uint64_t)llround(5.5 * ref_us);

    cap_result_t ign = cap_result(&s, CAP_IGN, now, ppr);
    printf("ign: n=%u period=%u low=%u fall=%.2f rise=%.2f\n", ign.count, ign.period_us,
           ign.low_us, ign.fall_deg, ign.rise_deg);
    check("ignition counted 6 pulses", ign.count == 6 && ign.valid);
    check("ignition period = one reference (35294 us)", ign.period_us >= 35293 && ign.period_us <= 35295);
    check("ignition low = 50 deg (9804 us)", ign.low_us >= 9803 && ign.low_us <= 9805);
    check("ignition fall at 40 deg", ign.have_angle && near(ign.fall_deg, 40.0f, 0.05f));
    check("ignition rise at 90 deg", near(ign.rise_deg, 90.0f, 0.05f));

    cap_result_t inj = cap_result(&s, CAP_INJ, now, ppr);
    printf("inj: n=%u period=%u low=%u fall=%.2f rise=%.2f\n", inj.count, inj.period_us,
           inj.low_us, inj.fall_deg, inj.rise_deg);
    check("injector counted 3 pulses", inj.count == 3);
    check("injector period = two references", inj.period_us >= 70587 && inj.period_us <= 70589);
    check("injector low = 2400 us", inj.low_us == 2400);
    check("injector fall at 10 deg", near(inj.fall_deg, 10.0f, 0.05f));

    // A pulse that straddles a reference: falls at 170 deg, rises at 200 deg.
    // Both edges must stay measured from the reference before the fall.
    uint64_t ref = T0 + (uint64_t)llround(6 * ref_us);
    cap_on_ref(&s, ref);
    cap_on_edge(&s, CAP_IGN, false, ref + (uint64_t)llround(170 * us_per_deg));
    cap_on_ref(&s, ref + (uint64_t)llround(ref_us));
    cap_on_edge(&s, CAP_IGN, true, ref + (uint64_t)llround(200 * us_per_deg));
    ign = cap_result(&s, CAP_IGN, ref + (uint64_t)llround(ref_us * 1.5), ppr);
    printf("straddle: fall=%.2f rise=%.2f\n", ign.fall_deg, ign.rise_deg);
    check("straddling pulse: fall 170, rise 200 (not 20)",
          near(ign.fall_deg, 170.0f, 0.05f) && near(ign.rise_deg, 200.0f, 0.05f));

    // Stale: nothing for more than 2 s.
    uint64_t late = ref + (uint64_t)llround(200 * us_per_deg) + CAP_STALE_US + 1;
    ign = cap_result(&s, CAP_IGN, late, ppr);
    check("stale after 2 s: invalid but count kept", !ign.valid && ign.count == 7);

    // No crank running: widths still valid, angles absent.
    cap_state_t nc;
    memset(&nc, 0, sizeof nc);
    cap_on_edge(&nc, CAP_INJ, true, 1000);  // rise before any fall: ignored
    cap_on_edge(&nc, CAP_INJ, false, 2000);
    cap_on_edge(&nc, CAP_INJ, true, 4400);
    inj = cap_result(&nc, CAP_INJ, 5000, ppr);
    check("no crank: pulse measured, no angle", inj.valid && inj.count == 1 && inj.low_us == 2400 &&
                                                !inj.have_angle && inj.period_us == 0);

    // Crank stopped and restarted: the stop is not a reference period.
    cap_state_t st;
    memset(&st, 0, sizeof st);
    cap_on_ref(&st, 1000000);
    cap_on_ref(&st, 1035294);
    cap_on_ref(&st, 31035294);  // 30 s later
    check("restart after a stop: no period until the next reference", st.ref_period_us == 0);
    cap_on_ref(&st, 31070588);
    check("second reference after the restart gives the period", st.ref_period_us == 35294);

    // 71.6 minutes after the last pulse (a 32-bit clock would have wrapped
    // back to "just now"): still stale.
    cap_state_t w;
    memset(&w, 0, sizeof w);
    cap_on_edge(&w, CAP_IGN, false, 1000);
    cap_on_edge(&w, CAP_IGN, true, 2000);
    ign = cap_result(&w, CAP_IGN, 2000 + 0x100000000ull + 500, ppr);
    check("reading 2^32 us old is stale, not valid again", !ign.valid);

    // One interrupt that latched both an ignition fall and the reference:
    // the fall is taken as just before the reference (~180 deg at 2 ppr).
    cap_state_t q;
    memset(&q, 0, sizeof q);
    const uint32_t none[CAP_COUNT] = {0, 0};
    const bool hi[CAP_COUNT] = {true, true}, lo[CAP_COUNT] = {false, true};
    cap_on_irq(&q, 1000000, true, none, hi);
    cap_on_irq(&q, 1035294, true, none, hi);
    const uint32_t ign_fall[CAP_COUNT] = {CAP_EV_FALL, 0};
    check("irq with reference reports it", cap_on_irq(&q, 1070588, true, ign_fall, lo));
    const uint32_t ign_rise[CAP_COUNT] = {CAP_EV_RISE, 0};
    cap_on_irq(&q, 1070588 + 9804, false, ign_rise, hi);
    ign = cap_result(&q, CAP_IGN, 1090000, ppr);
    check("fall latched with the reference counts against the previous one (~180 deg)",
          ign.have_angle && near(ign.fall_deg, 180.0f, 0.05f));

    // Both edges of one pin in one interrupt.
    const uint32_t both[CAP_COUNT] = {CAP_EV_FALL | CAP_EV_RISE, 0};
    uint32_t n_before = cap_result(&q, CAP_IGN, 1090000, ppr).count;
    cap_on_irq(&q, 1095000, false, both, hi);
    ign = cap_result(&q, CAP_IGN, 1095001, ppr);
    check("fall+rise with the pin high again: glitch counted, reading kept",
          ign.glitches == 1 && ign.count == n_before && ign.low_us == 9804);
    cap_on_irq(&q, 1100000, false, ign_fall, lo);
    cap_on_irq(&q, 1110000, false, both, lo);
    ign = cap_result(&q, CAP_IGN, 1110001, ppr);
    check("rise+fall with the pin low: pulse ended (10000 us) and the next began",
          ign.count == n_before + 1 && ign.low_us == 10000 && q.ch[CAP_IGN].fall_t == 1110000);

    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
