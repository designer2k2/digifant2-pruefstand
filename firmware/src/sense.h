#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;
    float bus_v;
    float current_a;
} ina_reading_t;

void sense_init(void);
ina_reading_t sense_read_ecu(void);    // U1, 0x40: total ECU current + ECU supply
ina_reading_t sense_read_valve(void);  // U3, 0x41: idle-valve current + valve supply

// Hardware-independent parts, sense_codec.c (INA226 datasheet SBOS547).
#define INA226_REG_CONFIG 0x00
#define INA226_REG_SHUNT 0x01
#define INA226_REG_BUS 0x02
#define INA226_REG_MANUFACTURER_ID 0xFE
#define INA226_MANUFACTURER_ID 0x5449

// Reserved bit 14 (reads 1) | 64 averages | bus 1.1 ms | shunt 1.1 ms |
// continuous shunt + bus. One averaged result every ~141 ms.
#define INA226_CONFIG 0x4727

float ina226_bus_volts(uint16_t raw);
float ina226_shunt_volts(uint16_t raw);
