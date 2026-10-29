#include "wave_core.h"

#include <stddef.h>

#include <math.h>

#define TWO_PI 6.2831853f
#define Q32 4294967296.0f

static uint32_t clamp_mv(float mv, uint32_t max_mv) {
    if (mv < 0.0f) return 0;
    if (mv > (float)max_mv) return max_mv;
    return (uint32_t)(mv + 0.5f);
}

static bool periodic(wave_shape_t s) {
    return s == WAVE_SINE || s == WAVE_TRIANGLE || s == WAVE_SQUARE || s == WAVE_LAMBDA;
}

const char *wave_shape_name(wave_shape_t s) {
    switch (s) {
    case WAVE_SINE: return "sine";
    case WAVE_TRIANGLE: return "tri";
    case WAVE_SQUARE: return "square";
    case WAVE_RAMP: return "ramp";
    case WAVE_LAMBDA: return "lam";
    case WAVE_LOOP: return "loop";
    default: return "off";
    }
}

const char *wave_validate(const wave_cfg_t *c, uint32_t max_mv) {
    if (c->shape == WAVE_OFF) return NULL;
    if (periodic(c->shape) && (c->freq_mhz < WAVE_FREQ_MIN_MHZ || c->freq_mhz > WAVE_FREQ_MAX_MHZ))
        return "freq must be 0.05..10 Hz";
    switch (c->shape) {
    case WAVE_SINE:
    case WAVE_TRIANGLE:
    case WAVE_SQUARE:
        if (c->offset < c->amp || c->offset + c->amp > max_mv)
            return "offset +- amp must stay within the channel range";
        if (c->shape == WAVE_SQUARE && (c->duty_pct < 1 || c->duty_pct > 99))
            return "duty must be 1..99";
        return NULL;
    case WAVE_RAMP:
        if (c->from_mv > max_mv || c->to_mv > max_mv) return "ramp ends must stay within the channel range";
        if (c->ramp_ms < 100 || c->ramp_ms > 3600000u) return "ramp time must be 0.1..3600 s";
        return NULL;
    case WAVE_LAMBDA:
        if (c->amp > 500) return "lambda amplitude must be 0..0.500";
        if (c->offset < 500 || c->offset > 1500) return "lambda offset must be 0.500..1.500";
        return NULL;
    case WAVE_LOOP:
        if (c->offset < 500 || c->offset > 1500) return "lambda0 must be 0.500..1.500";
        if (c->delay_ms > WAVE_LOOP_DELAY_MAX_MS) return "delay must be 0..1500 ms";
        if (c->ref_us && (c->ref_us < 100 || c->ref_us > 50000)) return "ref must be 100..50000 us";
        return NULL;
    default: return "unknown shape";
    }
}

uint32_t wave_phase_step(uint32_t freq_mhz, uint32_t dt_us) {
    // dt <= 100 ms keeps dt * freq * 2^32 below 2^64 (1e5 * 1e4 * 4.3e9 = 4.3e18).
    if (dt_us > 100000u) dt_us = 100000u;
    return (uint32_t)(((uint64_t)dt_us * freq_mhz * 4294967296ull) / 1000000000ull);
}

float wave_unit(wave_shape_t shape, uint32_t phase, uint32_t duty_pct) {
    float p = (float)phase / Q32;
    switch (shape) {
    case WAVE_SINE: return sinf(TWO_PI * p);
    case WAVE_TRIANGLE:
        if (p < 0.25f) return 4.0f * p;
        if (p < 0.75f) return 2.0f - 4.0f * p;
        return 4.0f * p - 4.0f;
    case WAVE_SQUARE: return p < (float)duty_pct / 100.0f ? 1.0f : -1.0f;
    default: return 0.0f;
    }
}

float wave_sensor_mv(float lambda) {
    return 450.0f + 450.0f * tanhf((1.0f - lambda) / 0.015f);
}

float wave_lag(float y, float x, uint32_t dt_us, uint32_t tau_ms) {
    float dt = (float)dt_us;
    float tau = (float)tau_ms * 1000.0f;
    return y + (x - y) * dt / (tau + dt);
}

void wave_state_reset(wave_state_t *s) {
    s->phase = 0;
    s->elapsed_us = 0;
    s->lp = 0.0f;
    s->lp_valid = false;
}

uint32_t wave_step(wave_state_t *s, const wave_cfg_t *c, uint32_t max_mv) {
    float mv = 0.0f;
    switch (c->shape) {
    case WAVE_SINE:
    case WAVE_TRIANGLE:
    case WAVE_SQUARE:
        mv = (float)c->offset + (float)c->amp * wave_unit(c->shape, s->phase, c->duty_pct);
        break;
    case WAVE_RAMP: {
        float total = (float)c->ramp_ms * 1000.0f;
        float f = (float)s->elapsed_us / total;
        if (f > 1.0f) f = 1.0f;
        mv = (float)c->from_mv + ((float)c->to_mv - (float)c->from_mv) * f;
        break;
    }
    case WAVE_LAMBDA: {
        float lambda = ((float)c->offset + (float)c->amp * wave_unit(WAVE_SINE, s->phase, 0)) / 1000.0f;
        float x = wave_sensor_mv(lambda);
        s->lp = s->lp_valid ? wave_lag(s->lp, x, WAVE_TICK_US, WAVE_SENSOR_TAU_MS) : x;
        s->lp_valid = true;
        mv = s->lp;
        if (max_mv > WAVE_LAMBDA_MAX_MV) max_mv = WAVE_LAMBDA_MAX_MV;
        break;
    }
    default: break;
    }
    s->phase += wave_phase_step(c->freq_mhz, WAVE_TICK_US);
    s->elapsed_us += WAVE_TICK_US;
    return clamp_mv(mv, max_mv);
}

void wave_loop_reset(wave_loop_t *l) {
    l->head = l->fill = 0;
    l->last_us = 0.0f;
    l->have_last = l->have_ref = false;
    l->ref_us = 0.0f;
    l->learn_n = 0;
    l->learn_sum = 0.0f;
    l->lp = 0.0f;
    l->lp_valid = false;
}

uint32_t wave_loop_step(wave_loop_t *l, const wave_cfg_t *c, bool inj_valid, uint32_t inj_us) {
    if (inj_valid) {
        l->last_us = (float)inj_us;
        l->have_last = true;
    }
    if (l->have_last) {
        l->ring[l->head] = l->last_us;
        l->head = (l->head + 1) % WAVE_LOOP_RING;
        if (l->fill < WAVE_LOOP_RING) l->fill++;
    }
    if (!l->have_ref) {
        if (c->ref_us) {
            l->ref_us = (float)c->ref_us;
            l->have_ref = true;
        } else if (l->have_last) {
            l->learn_sum += l->last_us;
            if (++l->learn_n >= WAVE_LOOP_LEARN_TICKS) {
                l->ref_us = l->learn_sum / (float)l->learn_n;
                l->have_ref = true;
            }
        }
    }
    float lambda = 1.0f;  // neutral until the reference is known
    if (l->have_ref && l->fill > 0) {
        uint32_t d = c->delay_ms * 1000u / WAVE_TICK_US;
        if (d > l->fill - 1) d = l->fill - 1;
        uint32_t idx = (l->head + WAVE_LOOP_RING - 1u - d) % WAVE_LOOP_RING;
        lambda = ((float)c->offset / 1000.0f) * l->ref_us / l->ring[idx];
        if (lambda < 0.5f) lambda = 0.5f;
        if (lambda > 2.0f) lambda = 2.0f;
    }
    float x = wave_sensor_mv(lambda);
    l->lp = l->lp_valid ? wave_lag(l->lp, x, WAVE_TICK_US, WAVE_SENSOR_TAU_MS) : x;
    l->lp_valid = true;
    return clamp_mv(l->lp, WAVE_LAMBDA_MAX_MV);
}
