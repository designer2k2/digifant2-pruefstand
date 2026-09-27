#include "sense.h"

// Bus: 1.25 mV per bit, bit 15 always 0. Shunt: two's complement, 2.5 uV per bit.
float ina226_bus_volts(uint16_t raw) { return (float)(raw & 0x7FFF) * 1.25e-3f; }

float ina226_shunt_volts(uint16_t raw) { return (float)(int16_t)raw * 2.5e-6f; }
