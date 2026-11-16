#pragma once

#include <stdbool.h>
#include <stdint.h>

// RS1 is 0.020 R, but U1's IN- sense trace taps the 0.6 mm power trace ~29 mm
// downstream of the shunt instead of its pad, so U1 also sees part of that
// trace's IR drop. Measured 2026-10-29: 7.32 mV across RS1 (0.366 A), BM869S in
// the + lead 0.367 A, U1 read 0.542 A -> 0.0296 R effective. Single point, not
// yet checked for linearity; remove once the layout is fixed (DESIGN.md).
#define RS1_EFFECTIVE_OHM 0.0296f

// INA226 shunt input range is +-81.92 mV; at/above this raw count the reading
// is clamped at full scale and the real current may be higher.
#define INA226_SHUNT_FULL_SCALE_RAW 0x7F00

typedef struct {
    bool valid;
    float bus_v;
    float current_a;
    bool full_scale;  // shunt input at or near its limit: current_a is a lower bound
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
