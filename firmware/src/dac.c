#include "dac.h"

#include <string.h>

// Stub: stores setpoints only. MCP4728 I2C writes come next.

static const char *channel_names[DAC_COUNT] = {
    [DAC_AIR] = "air",
    [DAC_WATER] = "water",
    [DAC_AFM] = "afm",
    [DAC_LAMBDA] = "lambda",
};

static uint32_t setpoint_mv[DAC_COUNT];

void dac_init(void) { memset(setpoint_mv, 0, sizeof setpoint_mv); }

bool dac_set_mv(dac_channel_t ch, uint32_t mv) {
    if (mv > DAC_MV_MAX) return false;
    setpoint_mv[ch] = mv;
    return true;
}

uint32_t dac_get_mv(dac_channel_t ch) { return setpoint_mv[ch]; }

const char *dac_channel_name(dac_channel_t ch) { return channel_names[ch]; }

bool dac_channel_from_name(const char *name, dac_channel_t *out) {
    for (int ch = 0; ch < DAC_COUNT; ch++) {
        if (strcmp(name, channel_names[ch]) == 0) {
            *out = (dac_channel_t)ch;
            return true;
        }
    }
    return false;
}

bool dac_is_implemented(void) { return false; }
