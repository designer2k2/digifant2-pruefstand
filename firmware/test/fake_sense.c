// Stand-in for sense.c (which needs I2C): fixed plausible readings for the ECU,
// and a missing valve INA226 so both output paths get exercised.
#include "sense.h"

void sense_init(void) {}

float fake_ecu_current_a = 0.5f;  // tests can change it

ina_reading_t sense_read_ecu(void) {
    return (ina_reading_t){.valid = true, .bus_v = 12.0f, .current_a = fake_ecu_current_a};
}

ina_reading_t sense_read_valve(void) { return (ina_reading_t){0}; }
