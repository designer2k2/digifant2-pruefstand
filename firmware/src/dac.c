#include "dac.h"

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pins.h"

#define I2C_TIMEOUT_US 5000

static uint32_t setpoint_mv[DAC_COUNT];
static bool last_ok;

static bool write_frames(const uint8_t *buf, size_t len) {
    int n = i2c_write_timeout_us(I2C_PORT, I2C_ADDR_DAC, buf, len, false, I2C_TIMEOUT_US);
    last_ok = n == (int)len;
    return last_ok;
}

// The chip powers up from its EEPROM, so set all four channels explicitly to
// 0 V with our reference/gain settings in one Multi-Write transaction.
void dac_init(void) {
    uint8_t buf[3 * DAC_COUNT];
    for (int ch = 0; ch < DAC_COUNT; ch++) {
        dac_encode((dac_channel_t)ch, 0, &buf[3 * ch]);
        setpoint_mv[ch] = 0;
    }
    write_frames(buf, sizeof buf);
}

dac_result_t dac_set_mv(dac_channel_t ch, uint32_t mv) {
    uint8_t frame[3];
    if (!dac_encode(ch, mv, frame)) return DAC_OUT_OF_RANGE;
    if (!write_frames(frame, sizeof frame)) return DAC_I2C_ERROR;
    setpoint_mv[ch] = mv;
    return DAC_OK;
}

uint32_t dac_get_mv(dac_channel_t ch) { return setpoint_mv[ch]; }

bool dac_i2c_ok(void) { return last_ok; }
