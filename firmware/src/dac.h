#pragma once

#include <stdbool.h>
#include <stdint.h>

// MCP4728 channel -> simulated sensor (DESIGN.md "dac_analog_sim").
typedef enum {
    DAC_AIR,     // VOUTA -> VW-9, intake-air NTC
    DAC_WATER,   // VOUTB -> VW-10, coolant NTC
    DAC_AFM,     // VOUTC -> VW-21, air-flow meter wiper
    DAC_LAMBDA,  // VOUTD -> VW-2, O2 sensor
    DAC_COUNT
} dac_channel_t;

#define DAC_MV_MAX 3300

void dac_init(void);
bool dac_set_mv(dac_channel_t ch, uint32_t mv);
uint32_t dac_get_mv(dac_channel_t ch);
const char *dac_channel_name(dac_channel_t ch);
bool dac_channel_from_name(const char *name, dac_channel_t *out);
bool dac_is_implemented(void);
