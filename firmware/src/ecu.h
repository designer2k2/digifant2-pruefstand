#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sense.h"

// ECU power with an over-current trip on U1 (DESIGN.md "ecu_power_switch").
// Everything that switches the ECU goes through here, never board.c directly.
//
// Trip: two INA226 results in a row above the limit switch the ECU off and
// latch a fault; `ecu on` is refused until `ecu reset`. U1 averages over
// ~141 ms, so this protects against a harness short or a failing ECU, not
// short spikes (F1 still covers those). If U1 gives ECU_SENSE_LOST_SAMPLES
// invalid results in a row the ECU is switched off and a `sense_lost` fault
// latches (also reported as tripped), cleared by `ecu reset`.
// A shunt reading at or near U1's full scale trips regardless of the limit.
#define ECU_TRIP_MA_DEFAULT 2400  // ECU ~0.6 A + cold idle valve ~1.8 A, under F1's 3 A
#define ECU_TRIP_MA_MIN 100
// 90 % of U1's +-81.92 mV range across the effective shunt (81.92 mV / ohm = mA), ~2490
#define ECU_TRIP_MA_MAX ((int)(81.92f / RS1_EFFECTIVE_OHM * 0.9f))
#define ECU_POLL_MS 150           // one new U1 result per poll
#define ECU_TRIP_SAMPLES 2
#define ECU_SENSE_LOST_SAMPLES 5  // ~750 ms without a valid U1 result

// Also parks the sensor DAC and knock outputs while the ECU is off, so they
// can't feed current into its unpowered inputs; switching on restores them.
bool ecu_set_power(bool on);  // false if on was refused because of a trip
bool ecu_get_power(void);
void ecu_poll(uint32_t now_ms);  // main loop

bool ecu_set_trip_ma(uint32_t ma);
uint32_t ecu_get_trip_ma(void);
bool ecu_tripped(void);  // any latched fault, over-current or sense lost
bool ecu_sense_lost(void);
uint32_t ecu_trip_reading_ma(void);  // U1 current that caused the trip
void ecu_reset_fault(void);
