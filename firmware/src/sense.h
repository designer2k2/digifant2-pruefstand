#pragma once

#include <stdbool.h>

typedef struct {
    bool valid;
    float bus_v;
    float current_a;
} ina_reading_t;

void sense_init(void);
ina_reading_t sense_read_ecu(void);    // U1, 0x40: total ECU current + ECU supply
ina_reading_t sense_read_valve(void);  // U3, 0x41: idle-valve current + valve supply
bool sense_is_implemented(void);
