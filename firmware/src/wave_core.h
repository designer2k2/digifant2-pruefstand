#pragma once

#include <stdbool.h>
#include <stdint.h>

// Waveform generator for the sensor-simulation DAC channels. Hardware-free:
// wave.c feeds it fixed ticks, the PC unit tests feed it simulated ones.
//
// Shapes:
//   sine / triangle / square   offset +- amp, in mV, at freq_mhz
//   ramp                       from_mv to to_mv over ramp_ms, then holds
//   lambda                     lambda(t) = offset +- amp (milli-lambda, sine) through
//                              the narrowband sensor curve and the sensor's lag
//   loop                       closed-loop emulation: lambda follows the injector
//                              open time the ECU produces (see wave_loop_step)
typedef enum {
    WAVE_OFF,
    WAVE_SINE,
    WAVE_TRIANGLE,
    WAVE_SQUARE,
    WAVE_RAMP,
    WAVE_LAMBDA,
    WAVE_LOOP,
} wave_shape_t;

#define WAVE_TICK_US 5000u          // fixed step, 200 Hz
#define WAVE_FREQ_MIN_MHZ 50u       // 0.05 Hz
#define WAVE_FREQ_MAX_MHZ 10000u    // 10 Hz
#define WAVE_LAMBDA_MAX_MV 1200u    // a narrowband sensor never exceeds ~1.1 V
#define WAVE_SENSOR_TAU_MS 100u     // response time of the simulated sensor
#define WAVE_LOOP_RING 320u         // 1.6 s of injector history at one entry per tick
#define WAVE_LOOP_DELAY_MAX_MS 1500u
#define WAVE_LOOP_LEARN_TICKS 200u  // 1 s to learn the reference injector time

typedef struct {
    wave_shape_t shape;
    uint32_t freq_mhz;   // millihertz (periodic shapes)
    uint32_t amp;        // peak: mV, or milli-lambda for WAVE_LAMBDA
    uint32_t offset;     // mV, or milli-lambda for WAVE_LAMBDA / lambda0 for WAVE_LOOP
    uint32_t duty_pct;   // square: percent of the period at +amp
    uint32_t from_mv, to_mv, ramp_ms;  // ramp
    uint32_t delay_ms;   // loop: transport delay engine -> sensor
    uint32_t ref_us;     // loop: injector time that means lambda0; 0 = learn it
} wave_cfg_t;

typedef struct {
    uint32_t phase;  // cycle fraction as 0..2^32
    uint64_t elapsed_us;
    float lp;        // sensor-lag filter state, mV
    bool lp_valid;
} wave_state_t;

typedef struct {
    float ring[WAVE_LOOP_RING];  // injector open time, one entry per tick
    uint32_t head, fill;
    float last_us;
    bool have_last, have_ref;
    float ref_us;
    uint32_t learn_n;
    float learn_sum;
    float lp;
    bool lp_valid;
} wave_loop_t;

const char *wave_shape_name(wave_shape_t s);

// NULL when the configuration is usable on a channel whose DAC allows max_mv,
// else a short reason.
const char *wave_validate(const wave_cfg_t *c, uint32_t max_mv);

// Cycle fraction added in dt_us at freq_mhz, as 0..2^32.
uint32_t wave_phase_step(uint32_t freq_mhz, uint32_t dt_us);
// -1..+1 for sine, triangle (starts at 0, rising) and square.
float wave_unit(wave_shape_t shape, uint32_t phase, uint32_t duty_pct);
// Narrowband zirconia sensor: about 450 mV at lambda 1, steep either side
// (rich > 0.8 V, lean < 0.1 V within +-0.03).
float wave_sensor_mv(float lambda);
// First-order lag of time constant tau_ms.
float wave_lag(float y, float x, uint32_t dt_us, uint32_t tau_ms);

void wave_state_reset(wave_state_t *s);
// Output in mV for this tick, then advances. Handles every shape but WAVE_LOOP.
uint32_t wave_step(wave_state_t *s, const wave_cfg_t *c, uint32_t max_mv);

void wave_loop_reset(wave_loop_t *l);
// Closed-loop emulation, one call per tick. The engine is modelled as
// lambda = lambda0 * ref / T_inj(t - delay): more fuel than at the reference
// gives a richer mixture, after the transport delay. The first second only
// learns ref (the mean injector time) while the output sits at lambda 1.
uint32_t wave_loop_step(wave_loop_t *l, const wave_cfg_t *c, bool inj_valid, uint32_t inj_us);
