// Stand-in for capture.c (which needs GPIO interrupts): a running ignition
// with angles, an injector that has never pulsed unless a test sets
// fake_inj_valid / fake_inj_low_us, and the real pulse-log ring.
#include "capture.h"

bool fake_inj_valid;
uint32_t fake_inj_low_us;

static pulse_log_t logs[CAP_COUNT];
static bool log_on;

void capture_init(void) {}

cap_result_t capture_get(cap_channel_t ch, uint32_t ppr) {
    (void)ppr;
    if (ch == CAP_IGN) {
        return (cap_result_t){.valid = true, .count = 42, .period_us = 35294, .low_us = 9804,
                              .have_angle = true, .fall_deg = 40.0f, .rise_deg = 90.0f};
    }
    return (cap_result_t){.valid = fake_inj_valid, .low_us = fake_inj_low_us,
                          .count = fake_inj_valid ? 1 : 0};
}

const char *capture_name(cap_channel_t ch) { return ch == CAP_IGN ? "ign" : "inj"; }

// What the interrupt does when a pulse completes while logging.
void fake_capture_pulse(cap_channel_t ch, uint32_t t_us, uint32_t low_us) {
    if (log_on) pulse_log_push(&logs[ch], t_us, low_us);
}

void capture_log_start(void) {
    for (int ch = 0; ch < CAP_COUNT; ch++) pulse_log_clear(&logs[ch]);
    log_on = true;
}
void capture_log_stop(void) { log_on = false; }
bool capture_log_running(void) { return log_on; }
uint32_t capture_log_read(cap_channel_t ch, pulse_rec_t *out, uint32_t max) {
    return pulse_log_pop(&logs[ch], out, max);
}
uint32_t capture_log_pending(cap_channel_t ch) { return pulse_log_pending(&logs[ch]); }
uint32_t capture_log_dropped(cap_channel_t ch) { return logs[ch].dropped; }
