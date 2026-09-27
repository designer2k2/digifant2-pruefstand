// Stand-in for capture.c (which needs GPIO interrupts): a running ignition
// with angles, and an injector that has never pulsed.
#include "capture.h"

void capture_init(void) {}

cap_result_t capture_get(cap_channel_t ch, uint32_t ppr) {
    (void)ppr;
    if (ch == CAP_IGN) {
        return (cap_result_t){.valid = true, .count = 42, .period_us = 35294, .low_us = 9804,
                              .have_angle = true, .fall_deg = 40.0f, .rise_deg = 90.0f};
    }
    return (cap_result_t){0};
}

const char *capture_name(cap_channel_t ch) { return ch == CAP_IGN ? "ign" : "inj"; }
