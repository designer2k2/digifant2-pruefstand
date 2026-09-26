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

typedef enum { DAC_OK, DAC_OUT_OF_RANGE, DAC_I2C_ERROR } dac_result_t;

// All values are the DAC output pin voltage, before the series resistor.
void dac_init(void);
dac_result_t dac_set_mv(dac_channel_t ch, uint32_t mv);
uint32_t dac_get_mv(dac_channel_t ch);
bool dac_i2c_ok(void);  // result of the most recent DAC transaction

// Hardware-independent parts, dac_codec.c.
uint32_t dac_max_mv(dac_channel_t ch);
const char *dac_channel_name(dac_channel_t ch);
bool dac_channel_from_name(const char *name, dac_channel_t *out);
// Builds the 3-byte MCP4728 Multi-Write frame for one channel (datasheet
// DS22187E, figure 5-8). Returns false if mv is out of range for the channel.
bool dac_encode(dac_channel_t ch, uint32_t mv, uint8_t frame[3]);
