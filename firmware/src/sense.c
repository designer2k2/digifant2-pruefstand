#include "sense.h"

// Stub: no INA226 access yet, every reading is marked invalid.

void sense_init(void) {}

ina_reading_t sense_read_ecu(void) { return (ina_reading_t){0}; }

ina_reading_t sense_read_valve(void) { return (ina_reading_t){0}; }

bool sense_is_implemented(void) { return false; }
