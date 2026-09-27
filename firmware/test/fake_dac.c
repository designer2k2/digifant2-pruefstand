// Stand-in for dac.c (which needs I2C): real range checks via dac_codec.c, no bus.
#include "dac.h"

static uint32_t setpoint_mv[DAC_COUNT];

void dac_init(void) {}

dac_result_t dac_set_mv(dac_channel_t ch, uint32_t mv) {
    uint8_t frame[3];
    if (!dac_encode(ch, mv, frame)) return DAC_OUT_OF_RANGE;
    setpoint_mv[ch] = mv;
    return DAC_OK;
}

uint32_t dac_get_mv(dac_channel_t ch) { return setpoint_mv[ch]; }

bool dac_i2c_ok(void) { return true; }

void dac_poll(uint32_t now_ms) { (void)now_ms; }
void dac_set_output_enabled(bool enabled) { (void)enabled; }
