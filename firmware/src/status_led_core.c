#include "status_led_core.h"

status_rgb_t status_led_pick(bool ecu_on, bool ecu_tripped, uint32_t rpm) {
    if (ecu_tripped) return (status_rgb_t){STATUS_LED_LEVEL, 0, 0};
    if (!ecu_on) return (status_rgb_t){4, 4, 4};
    if (rpm > 0) return (status_rgb_t){0, 0, STATUS_LED_LEVEL};
    return (status_rgb_t){0, STATUS_LED_LEVEL, 0};
}
