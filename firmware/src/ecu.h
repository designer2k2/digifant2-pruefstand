#pragma once

#include <stdbool.h>
#include <stdint.h>

// ECU power with an over-current trip on U1 (DESIGN.md "ecu_power_switch").
// Everything that switches the ECU goes through here, never board.c directly.
//
// Trip: two INA226 results in a row above the limit switch the ECU off and
// latch a fault; `ecu on` is refused until `ecu reset`. U1 averages over
// ~141 ms, so this protects against a harness short or a failing ECU, not
// short spikes (F1 still covers those). If U1 isn't answering there is no
// trip; `read` then shows ecu_a=na.
#define ECU_TRIP_MA_DEFAULT 2500  // ECU ~0.6 A + cold idle valve ~1.8 A, under F1's 3 A
#define ECU_TRIP_MA_MIN 100
#define ECU_TRIP_MA_MAX 4000      // U1's range is 4.09 A
#define ECU_POLL_MS 150           // one new U1 result per poll
#define ECU_TRIP_SAMPLES 2

// Also parks the sensor DAC and knock outputs while the ECU is off, so they
// can't feed current into its unpowered inputs; switching on restores them.
bool ecu_set_power(bool on);  // false if on was refused because of a trip
bool ecu_get_power(void);
void ecu_poll(uint32_t now_ms);  // main loop

bool ecu_set_trip_ma(uint32_t ma);
uint32_t ecu_get_trip_ma(void);
bool ecu_tripped(void);
uint32_t ecu_trip_reading_ma(void);  // U1 current that caused the trip
void ecu_reset_fault(void);
