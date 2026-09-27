#include "capture.h"

void cap_on_ref(cap_state_t *s, uint64_t t) {
    // A gap longer than CAP_STALE_US means the crank was stopped: don't use
    // the stop as a period, wait for the next reference instead.
    uint64_t gap = t - s->ref_t;
    s->ref_period_us = s->have_ref && gap <= CAP_STALE_US ? (uint32_t)gap : 0;
    s->ref_t = t;
    s->have_ref = true;
}

void cap_on_edge(cap_state_t *s, cap_channel_t ch, bool rising, uint64_t t) {
    cap_chan_state_t *c = &s->ch[ch];
    if (!rising) {
        c->period_us = c->have_fall && t - c->fall_t <= CAP_STALE_US ? (uint32_t)(t - c->fall_t) : 0;
        c->fall_t = t;
        c->have_fall = true;
        // Both edges are measured from the reference preceding the falling edge,
        // so a reference landing mid-pulse can't make the rise read earlier.
        uint64_t d = t - s->ref_t;
        c->has_ref = s->have_ref && s->ref_period_us != 0 && d <= 2 * (uint64_t)s->ref_period_us;
        c->ref_dly_us = (uint32_t)d;
        c->ref_period_us = s->ref_period_us;
        return;
    }
    if (!c->have_fall) return;  // pulse started before we were watching
    c->low_us = (uint32_t)(t - c->fall_t);
    c->last_end_t = t;
    c->count++;
}

bool cap_on_irq(cap_state_t *s, uint64_t t, bool ref, const uint32_t ev[CAP_COUNT],
                const bool high[CAP_COUNT]) {
    // ECU edges first: one latched together with the reference is taken as
    // just before it (angle ~360/ppr) rather than just after (~0).
    for (int ch = 0; ch < CAP_COUNT; ch++) {
        cap_chan_state_t *c = &s->ch[ch];
        switch (ev[ch] & (CAP_EV_FALL | CAP_EV_RISE)) {
        case CAP_EV_FALL: cap_on_edge(s, (cap_channel_t)ch, false, t); break;
        case CAP_EV_RISE: cap_on_edge(s, (cap_channel_t)ch, true, t); break;
        case CAP_EV_FALL | CAP_EV_RISE:
            // Both edges since the last interrupt, order unknown from the
            // latches alone. Low now: a pulse ended and the next began, in
            // that order. High now: a pulse shorter than our latency, i.e. a
            // glitch; counted, but not allowed to overwrite the last reading.
            if (!high[ch]) {
                cap_on_edge(s, (cap_channel_t)ch, true, t);
                cap_on_edge(s, (cap_channel_t)ch, false, t);
            } else {
                c->glitches++;
            }
            break;
        default: break;
        }
    }
    if (ref) cap_on_ref(s, t);
    return ref;
}

static float to_deg(uint32_t us, uint32_t ref_period_us, uint32_t ppr) {
    // One reference period is 1/ppr of a crank revolution.
    return (float)us / (float)ref_period_us * (360.0f / (float)ppr);
}

cap_result_t cap_result(const cap_state_t *s, cap_channel_t ch, uint64_t now, uint32_t ppr) {
    const cap_chan_state_t *c = &s->ch[ch];
    cap_result_t r = {0};
    r.count = c->count;
    r.glitches = c->glitches;
    if (c->count == 0 || now - c->last_end_t > CAP_STALE_US) return r;

    r.valid = true;
    r.period_us = c->period_us;
    r.low_us = c->low_us;
    if (ppr > 0 && c->has_ref) {
        r.have_angle = true;
        r.fall_deg = to_deg(c->ref_dly_us, c->ref_period_us, ppr);
        r.rise_deg = to_deg(c->ref_dly_us + c->low_us, c->ref_period_us, ppr);
    }
    return r;
}
