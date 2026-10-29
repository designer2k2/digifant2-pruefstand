#include "wave.h"

#include <stddef.h>

#include "capture.h"
#include "crank.h"
#include "ecu.h"

// If the main loop stalls longer than this the schedule is restarted instead
// of replaying the missed ticks.
#define MAX_CATCH_UP_US 100000u

typedef struct {
    wave_cfg_t cfg;
    wave_state_t st;
    bool active;
    bool started;        // first tick done, t0 known
    bool written;
    uint32_t sent_mv;    // what we last sent to the DAC
    uint32_t t0_us;
} chan_t;

static chan_t chans[DAC_COUNT];
static wave_loop_t loop_state;  // only the lambda channel can run a loop
static uint64_t next_tick_us;
static bool clock_running;

uint32_t wave_max_mv(dac_channel_t ch) {
    uint32_t m = dac_max_mv(ch);
    return ch == DAC_LAMBDA && m > WAVE_LAMBDA_MAX_MV ? WAVE_LAMBDA_MAX_MV : m;
}

const char *wave_start(dac_channel_t ch, const wave_cfg_t *cfg) {
    if ((cfg->shape == WAVE_LAMBDA || cfg->shape == WAVE_LOOP) && ch != DAC_LAMBDA)
        return "lam and loop only run on the lambda channel";
    const char *why = wave_validate(cfg, wave_max_mv(ch));
    if (why) return why;
    chan_t *c = &chans[ch];
    c->cfg = *cfg;
    c->active = cfg->shape != WAVE_OFF;
    c->started = false;
    c->written = false;
    wave_state_reset(&c->st);
    if (cfg->shape == WAVE_LOOP) wave_loop_reset(&loop_state);
    return NULL;
}

void wave_stop(dac_channel_t ch) {
    chans[ch].active = false;
    chans[ch].cfg.shape = WAVE_OFF;
}

void wave_stop_all(void) {
    for (int ch = 0; ch < DAC_COUNT; ch++) wave_stop((dac_channel_t)ch);
}

bool wave_active(dac_channel_t ch) { return chans[ch].active; }
wave_cfg_t wave_get(dac_channel_t ch) { return chans[ch].cfg; }

uint32_t wave_t0_us(dac_channel_t ch) {
    // Not ticked yet: the next scheduled tick will be phase 0.
    return chans[ch].started ? chans[ch].t0_us : (uint32_t)next_tick_us;
}

static void tick(uint64_t t_us, bool write_now) {
    for (int i = 0; i < DAC_COUNT; i++) {
        chan_t *c = &chans[i];
        if (!c->active) continue;
        dac_channel_t ch = (dac_channel_t)i;
        // Somebody else wrote this channel: their value wins.
        if (c->written && dac_get_mv(ch) != c->sent_mv) {
            wave_stop(ch);
            continue;
        }
        if (!c->started) {
            c->started = true;
            c->t0_us = (uint32_t)t_us;
        }
        uint32_t mv;
        if (c->cfg.shape == WAVE_LOOP) {
            cap_result_t inj = capture_get(CAP_INJ, crank_get_ppr());
            mv = wave_loop_step(&loop_state, &c->cfg, inj.valid, inj.low_us);
        } else {
            mv = wave_step(&c->st, &c->cfg, wave_max_mv(ch));
        }
        // Only the last tick of a catch-up burst touches the bus; and nothing
        // while the ECU is off.
        if (!write_now || !ecu_get_power()) continue;
        if (c->written && c->sent_mv == mv) continue;
        if (dac_set_mv(ch, mv) == DAC_OK) {
            c->sent_mv = mv;
            c->written = true;
        }
    }
}

void wave_poll(uint64_t now_us) {
    if (!clock_running || (int64_t)(now_us - next_tick_us) > (int64_t)MAX_CATCH_UP_US) {
        next_tick_us = now_us;
        clock_running = true;
    }
    while ((int64_t)(now_us - next_tick_us) >= 0) {
        uint64_t t = next_tick_us;
        next_tick_us += WAVE_TICK_US;
        tick(t, (int64_t)(now_us - next_tick_us) < 0);
    }
}
