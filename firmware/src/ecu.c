#include "ecu.h"

#include "board.h"
#include "dac.h"
#include "knock.h"
#include "sense.h"

static uint32_t trip_ma = ECU_TRIP_MA_DEFAULT;
static bool tripped;
static uint32_t trip_reading_ma;
static uint32_t over_count;
static uint32_t next_poll_ms;

// Inputs are only driven while the ECU is powered: switch on first, then the
// outputs; park the outputs first, then switch off.
static void power_off(void) {
    dac_set_output_enabled(false);
    knock_set_output_enabled(false);
    board_set_ecu_power(false);
}

bool ecu_set_power(bool on) {
    if (!on) {
        power_off();
        return true;
    }
    if (tripped) return false;
    over_count = 0;
    board_set_ecu_power(true);
    dac_set_output_enabled(true);
    knock_set_output_enabled(true);
    return true;
}

bool ecu_get_power(void) { return board_get_ecu_power(); }

void ecu_poll(uint32_t now_ms) {
    if (!board_get_ecu_power() || (int32_t)(now_ms - next_poll_ms) < 0) return;
    next_poll_ms = now_ms + ECU_POLL_MS;

    ina_reading_t r = sense_read_ecu();
    if (!r.valid) return;
    uint32_t ma = r.current_a > 0 ? (uint32_t)(r.current_a * 1000.0f) : 0;
    if (ma <= trip_ma) {
        over_count = 0;
        return;
    }
    if (++over_count < ECU_TRIP_SAMPLES) return;
    power_off();
    tripped = true;
    trip_reading_ma = ma;
}

bool ecu_set_trip_ma(uint32_t ma) {
    if (ma < ECU_TRIP_MA_MIN || ma > ECU_TRIP_MA_MAX) return false;
    trip_ma = ma;
    return true;
}

uint32_t ecu_get_trip_ma(void) { return trip_ma; }
bool ecu_tripped(void) { return tripped; }
uint32_t ecu_trip_reading_ma(void) { return trip_reading_ma; }

void ecu_reset_fault(void) {
    tripped = false;
    trip_reading_ma = 0;
    over_count = 0;
}
