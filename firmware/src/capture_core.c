#include "capture.h"

void cap_on_ref(cap_state_t *s, uint32_t t) {
    if (s->have_ref) s->ref_period_us = t - s->ref_t;
    s->ref_t = t;
    s->have_ref = true;
}

void cap_on_edge(cap_state_t *s, cap_channel_t ch, bool rising, uint32_t t) {
    cap_chan_state_t *c = &s->ch[ch];
    if (!rising) {
        if (c->have_fall) c->period_us = t - c->fall_t;
        c->fall_t = t;
        c->have_fall = true;
        // Both edges are measured from the reference preceding the falling edge,
        // so a reference landing mid-pulse can't make the rise read earlier.
        uint32_t d = t - s->ref_t;
        c->has_ref = s->have_ref && s->ref_period_us != 0 && d <= s->ref_period_us * 2;
        c->ref_dly_us = d;
        c->ref_period_us = s->ref_period_us;
        return;
    }
    if (!c->have_fall) return;  // pulse started before we were watching
    c->low_us = t - c->fall_t;
    c->last_end_t = t;
    c->count++;
}

static float to_deg(uint32_t us, uint32_t ref_period_us, uint32_t ppr) {
    // One reference period is 1/ppr of a crank revolution.
    return (float)us / (float)ref_period_us * (360.0f / (float)ppr);
}

cap_result_t cap_result(const cap_state_t *s, cap_channel_t ch, uint32_t now, uint32_t ppr) {
    const cap_chan_state_t *c = &s->ch[ch];
    cap_result_t r = {0};
    r.count = c->count;
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
