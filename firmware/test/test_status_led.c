// Colour rules for the status pixel (status_led_core.c).
#include <stdio.h>

#include "status_led_core.h"

static int failures;

static void check(const char *what, int ok) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static int is(status_rgb_t c, int r, int g, int b) { return c.r == r && c.g == g && c.b == b; }

int main(void) {
    int L = STATUS_LED_LEVEL;
    check("ecu off: dim white", is(status_led_pick(false, false, 0), 4, 4, 4));
    check("ecu off, crank value set: still dim white",
          is(status_led_pick(false, false, 850), 4, 4, 4));
    check("ecu on, crank stopped: green", is(status_led_pick(true, false, 0), 0, L, 0));
    check("ecu on, crank running: blue", is(status_led_pick(true, false, 850), 0, 0, L));
    check("trip: red", is(status_led_pick(false, true, 0), L, 0, 0));
    check("trip wins over a running crank", is(status_led_pick(true, true, 3000), L, 0, 0));
    check("levels are dim", L <= 64);
    return failures ? 1 : 0;
}
