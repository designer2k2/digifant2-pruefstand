// Over-current trip and output parking in ecu.c, against the in-memory board
// and a settable ECU current (fake_sense.c). The DAC and knock hooks are
// recorded here so the switching order can be checked.
#include <stdio.h>

#include "board.h"
#include "dac.h"
#include "ecu.h"
#include "knock.h"

extern float fake_ecu_current_a;

static int failures;
static bool dac_on, knock_on;
static bool inputs_driven_while_off;

static void check(const char *what, int ok) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

void dac_set_output_enabled(bool e) {
    dac_on = e;
    if (e && !board_get_ecu_power()) inputs_driven_while_off = true;
}
void knock_set_output_enabled(bool e) {
    knock_on = e;
    if (e && !board_get_ecu_power()) inputs_driven_while_off = true;
}

int main(void) {
    check("default trip 2500 mA", ecu_get_trip_ma() == ECU_TRIP_MA_DEFAULT);
    check("trip limits 100..4000 mA",
          !ecu_set_trip_ma(99) && !ecu_set_trip_ma(4001) && ecu_set_trip_ma(4000) &&
              ecu_set_trip_ma(ECU_TRIP_MA_DEFAULT));

    check("ecu on", ecu_set_power(true) && board_get_ecu_power());
    check("on: DAC and knock outputs released, only after the rail is up",
          dac_on && knock_on && !inputs_driven_while_off);
    ecu_set_power(false);
    check("off: outputs parked and rail off", !dac_on && !knock_on && !board_get_ecu_power());

    uint32_t t = 0;
    ecu_set_power(true);
    fake_ecu_current_a = 0.6f;
    for (int i = 0; i < 10; i++) ecu_poll(t += ECU_POLL_MS);
    check("0.6 A: stays on", board_get_ecu_power() && !ecu_tripped());

    fake_ecu_current_a = 3.0f;
    ecu_poll(t += ECU_POLL_MS);
    check("one sample over the limit: still on", board_get_ecu_power());
    fake_ecu_current_a = 0.6f;
    ecu_poll(t += ECU_POLL_MS);
    fake_ecu_current_a = 3.0f;
    ecu_poll(t += ECU_POLL_MS);
    check("isolated samples over the limit don't add up", board_get_ecu_power());

    ecu_poll(t += 10);
    check("polls closer than one INA226 result are skipped", board_get_ecu_power());
    ecu_poll(t += ECU_POLL_MS);
    check("two samples in a row over the limit: tripped, off, outputs parked",
          !board_get_ecu_power() && ecu_tripped() && !dac_on && !knock_on);
    check("trip reading recorded", ecu_trip_reading_ma() == 3000);

    fake_ecu_current_a = 0.6f;
    check("ecu on refused while tripped", !ecu_set_power(true) && !board_get_ecu_power());
    check("ecu off still allowed while tripped", ecu_set_power(false));
    ecu_reset_fault();
    check("after reset: on works again", ecu_set_power(true) && board_get_ecu_power());

    ecu_set_power(false);
    fake_ecu_current_a = 3.0f;
    for (int i = 0; i < 5; i++) ecu_poll(t += ECU_POLL_MS);
    check("no trip while the ECU is off", !ecu_tripped());

    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
