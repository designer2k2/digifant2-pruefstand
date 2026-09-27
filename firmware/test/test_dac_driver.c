// dac.c on the simulated SDK: outputs parked at 0 V while the ECU is off,
// setpoints kept and restored, and a missed write retried until the chip
// holds the current values again.
#include <stdio.h>

#include "dac.h"
#include "sim.h"

static int failures;

static void check(const char *what, int ok) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

// Code in the chip for a channel, from the last Multi-Write (all 4 channels).
static uint32_t chip_code(int ch) {
    if (sim_i2c_last_len != 12) return 0xFFFF;
    return (uint32_t)(sim_i2c_last[3 * ch + 1] & 0x0F) << 8 | sim_i2c_last[3 * ch + 2];
}

int main(void) {
    dac_init();
    check("init writes all four channels at 0", sim_i2c_last_len == 12 && chip_code(DAC_AIR) == 0);

    check("set while parked: accepted, stored", dac_set_mv(DAC_AIR, 1234) == DAC_OK &&
                                                   dac_get_mv(DAC_AIR) == 1234);
    check("...but the chip stays at 0 V", sim_i2c_last_len == 12 && chip_code(DAC_AIR) == 0);

    dac_set_output_enabled(true);
    check("ECU on: setpoints restored in one transaction",
          sim_i2c_last_len == 12 && chip_code(DAC_AIR) == 1234);
    dac_set_mv(DAC_WATER, 2000);
    check("enabled: single-channel write", sim_i2c_last_len == 3);

    dac_set_output_enabled(false);
    check("ECU off: all channels back to 0", sim_i2c_last_len == 12 && chip_code(DAC_AIR) == 0 &&
                                                 chip_code(DAC_WATER) == 0);
    dac_set_output_enabled(true);

    sim_i2c_fail = true;
    check("failed write: error, old setpoint kept",
          dac_set_mv(DAC_AFM, 1500) == DAC_I2C_ERROR && dac_get_mv(DAC_AFM) == 0 && !dac_i2c_ok());
    size_t n = sim_i2c_writes;
    dac_poll(100);
    dac_poll(500);
    check("retry at most once a second while the chip is missing", sim_i2c_writes == n + 1);
    sim_i2c_fail = false;
    dac_poll(1200);
    check("chip back: everything rewritten, status ok",
          dac_i2c_ok() && sim_i2c_last_len == 12 && chip_code(DAC_AIR) == 1234);

    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
