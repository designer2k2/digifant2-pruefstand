// Stand-in for sense.c (which needs I2C): fixed plausible readings for the ECU,
// and a missing valve INA226 so both output paths get exercised.
#include "sense.h"

void sense_init(void) {}

ina_reading_t sense_read_ecu(void) {
    return (ina_reading_t){.valid = true, .bus_v = 12.0f, .current_a = 0.5f};
}

ina_reading_t sense_read_valve(void) { return (ina_reading_t){0}; }
