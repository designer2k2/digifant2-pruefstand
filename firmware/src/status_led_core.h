#pragma once

#include <stdbool.h>
#include <stdint.h>

// What the single SK6812 pixel (D2) shows while a test runs. Kept hardware-free
// so the colour rules can be unit-tested on the PC.
//
//   red    ECU over-current trip latched (fix, then `ecu reset`)
//   blue   ECU on and the crank signal running (simulator active)
//   green  ECU on, crank stopped (powered, waiting)
//   dim white  ECU off (safe)
//
// Levels are low on purpose: the 5050 package is very bright at full scale.
typedef struct {
    uint8_t r, g, b;
} status_rgb_t;

#define STATUS_LED_LEVEL 24

status_rgb_t status_led_pick(bool ecu_on, bool ecu_tripped, uint32_t rpm);
