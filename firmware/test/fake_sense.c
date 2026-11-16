// Stand-in for sense.c (which needs I2C): fixed plausible readings for the ECU,
// and a missing valve INA226 so both output paths get exercised.
#include "sense.h"

void sense_init(void) {}

float fake_ecu_current_a = 0.5f;  // tests can change it

bool fake_ecu_full_scale;  // U1's shunt input saturated
bool fake_ecu_invalid;     // U1 not answering

ina_reading_t sense_read_ecu(void) {
    return (ina_reading_t){.valid = !fake_ecu_invalid,
                           .bus_v = 12.0f,
                           .current_a = fake_ecu_current_a,
                           .full_scale = fake_ecu_full_scale};
}

ina_reading_t sense_read_valve(void) { return (ina_reading_t){0}; }
