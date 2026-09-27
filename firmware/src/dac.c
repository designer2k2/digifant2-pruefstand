#include "dac.h"

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pins.h"

#define I2C_TIMEOUT_US 5000
#define RETRY_MS 1000

static uint32_t setpoint_mv[DAC_COUNT];
static bool output_enabled;  // false while the ECU is off: all channels at 0 V
static bool in_sync;         // the chip is known to hold what we last sent
static bool last_ok;

static bool write_frames(const uint8_t *buf, size_t len) {
    int n = i2c_write_timeout_us(I2C_PORT, I2C_ADDR_DAC, buf, len, false, I2C_TIMEOUT_US);
    last_ok = n == (int)len;
    if (!last_ok) in_sync = false;
    return last_ok;
}

// Writes all four channels in one Multi-Write transaction: the setpoints, or
// 0 V while parked. The chip powers up from its EEPROM, so this is also what
// makes sure every channel carries our reference/gain settings.
static bool write_all(void) {
    uint8_t buf[3 * DAC_COUNT];
    for (int ch = 0; ch < DAC_COUNT; ch++) {
        dac_encode((dac_channel_t)ch, output_enabled ? setpoint_mv[ch] : 0, &buf[3 * ch]);
    }
    in_sync = write_frames(buf, sizeof buf);
    return in_sync;
}

void dac_init(void) { write_all(); }

void dac_poll(uint32_t now_ms) {
    static uint32_t next_retry_ms;
    if (in_sync || (int32_t)(now_ms - next_retry_ms) < 0) return;
    next_retry_ms = now_ms + RETRY_MS;
    write_all();
}

dac_result_t dac_set_mv(dac_channel_t ch, uint32_t mv) {
    uint8_t frame[3];
    if (!dac_encode(ch, mv, frame)) return DAC_OUT_OF_RANGE;
    if (output_enabled && in_sync) {
        if (!write_frames(frame, sizeof frame)) return DAC_I2C_ERROR;
        setpoint_mv[ch] = mv;
        return DAC_OK;
    }
    // Parked, or the chip missed an earlier write: store the value and bring
    // the whole chip up to date (0 V everywhere while parked).
    uint32_t old = setpoint_mv[ch];
    setpoint_mv[ch] = mv;
    if (!write_all()) {
        setpoint_mv[ch] = old;
        return DAC_I2C_ERROR;
    }
    return DAC_OK;
}

uint32_t dac_get_mv(dac_channel_t ch) { return setpoint_mv[ch]; }

bool dac_i2c_ok(void) { return last_ok && in_sync; }

void dac_set_output_enabled(bool enabled) {
    output_enabled = enabled;
    write_all();
}
