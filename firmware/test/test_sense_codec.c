// Checks INA226 raw-register conversions (SBOS547 tables 7-7/7-8) and the
// shunt-to-current math sense.c uses for RS1 (0.020 R) and RS2 (0.033 R).
#include <math.h>
#include <stdio.h>

#include "sense.h"

static int failures;

static void expect(const char *what, float got, float want) {
    int ok = fabsf(got - want) <= fabsf(want) * 1e-5f + 1e-9f;
    printf("%s %-44s got %.6f want %.6f\n", ok ? "ok  " : "FAIL", what, got, want);
    if (!ok) failures++;
}

int main(void) {
    expect("bus 0x2580 (9600 * 1.25 mV)", ina226_bus_volts(0x2580), 12.0f);
    expect("bus 0x7FFF full scale", ina226_bus_volts(0x7FFF), 40.95875f);
    expect("bus bit 15 ignored", ina226_bus_volts(0x8000 | 0x2580), 12.0f);

    expect("shunt 0x0FA0 (4000 * 2.5 uV)", ina226_shunt_volts(0x0FA0), 0.010f);
    expect("shunt 0xF060 (-4000, two's complement)", ina226_shunt_volts(0xF060), -0.010f);
    expect("shunt 0x7FFF full scale", ina226_shunt_volts(0x7FFF), 0.0819175f);
    expect("shunt 0x8000 negative full scale", ina226_shunt_volts(0x8000), -0.08192f);

    // Current as sense.c computes it: sign * V_shunt / R_shunt.
    expect("ECU  10 mV / 0.020 R", ina226_shunt_volts(0x0FA0) / 0.020f, 0.5f);
    expect("ECU  full scale / 0.020 R", ina226_shunt_volts(0x7FFF) / 0.020f, 4.095875f);
    expect("valve -10 mV (IN+ on ECU side) -> positive", -1.0f * ina226_shunt_volts(0xF060) / 0.033f,
           0.30303030f);
    expect("valve full scale / 0.033 R", ina226_shunt_volts(0x8000) / 0.033f * -1.0f, 2.4824242f);

    expect("config word", (float)INA226_CONFIG, (float)(0x4000 | 3 << 9 | 4 << 6 | 4 << 3 | 7));

    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
