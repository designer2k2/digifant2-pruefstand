#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dac.h"
#include "wave_core.h"

// Runs the waveform generator on the sensor DAC channels. One fixed 200 Hz
// tick; each active channel computes its next value and writes it to the DAC
// only when the millivolt value changed. Nothing is written while the ECU is
// off (the DAC is parked at 0 V then); the waves keep their phase and carry on
// when it comes back.
//
// A wave ends when someone else writes the channel (`dac`, the buttons) or
// with wave_stop(). The lambda channel is capped at WAVE_LAMBDA_MAX_MV.

void wave_poll(uint64_t now_us);  // main loop

// Starts (or replaces) the wave on a channel. Returns NULL, or a reason it was refused.
const char *wave_start(dac_channel_t ch, const wave_cfg_t *cfg);
void wave_stop(dac_channel_t ch);
void wave_stop_all(void);
bool wave_active(dac_channel_t ch);
wave_cfg_t wave_get(dac_channel_t ch);
// Time of the tick at which the wave started (phase 0), low 32 bits of the
// microsecond clock; the PC uses it to put the wave's phase on the pulse log's clock.
uint32_t wave_t0_us(dac_channel_t ch);
// Largest mV the generator may write on a channel.
uint32_t wave_max_mv(dac_channel_t ch);
