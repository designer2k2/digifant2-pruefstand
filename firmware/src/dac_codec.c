#include <string.h>

#include "dac.h"

// Every channel uses the internal 2.048 V reference, which is more accurate than
// the Pico's 3V3 rail. Gain x2 gives exactly 1 mV per code (output capped at
// VDD); lambda uses gain x1 for 0.5 mV steps over 0-2.048 V.
typedef struct {
    const char *name;
    bool gain2;
    uint32_t max_mv;
} channel_cfg_t;

static const channel_cfg_t cfg[DAC_COUNT] = {
    [DAC_AIR]    = {"air",    true,  3300},
    [DAC_WATER]  = {"water",  true,  3300},
    [DAC_AFM]    = {"afm",    true,  3300},
    [DAC_LAMBDA] = {"lambda", false, 2047},
};

#define MCP4728_MULTI_WRITE 0x40
#define MCP4728_VREF_INTERNAL (1u << 7)
#define MCP4728_GAIN_X2 (1u << 4)

uint32_t dac_max_mv(dac_channel_t ch) { return cfg[ch].max_mv; }

const char *dac_channel_name(dac_channel_t ch) { return cfg[ch].name; }

bool dac_channel_from_name(const char *name, dac_channel_t *out) {
    for (int ch = 0; ch < DAC_COUNT; ch++) {
        if (strcmp(name, cfg[ch].name) == 0) {
            *out = (dac_channel_t)ch;
            return true;
        }
    }
    return false;
}

bool dac_encode(dac_channel_t ch, uint32_t mv, uint8_t frame[3]) {
    if (mv > cfg[ch].max_mv) return false;
    uint32_t code = cfg[ch].gain2 ? mv : mv * 2;
    // UDAC = 0: update the output as soon as the frame is acknowledged.
    frame[0] = (uint8_t)(MCP4728_MULTI_WRITE | (ch << 1));
    // PD1:PD0 = 00, normal mode.
    frame[1] = (uint8_t)(MCP4728_VREF_INTERNAL | (cfg[ch].gain2 ? MCP4728_GAIN_X2 : 0) |
                         (code >> 8));
    frame[2] = (uint8_t)(code & 0xFF);
    return true;
}
