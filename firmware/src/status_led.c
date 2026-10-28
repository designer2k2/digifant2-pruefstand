#include "status_led.h"

#include "crank.h"
#include "ecu.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pins.h"
#include "status_led.pio.h"
#include "status_led_core.h"

#define LED_PIO pio1
#define LED_SM 0
// The pixel keeps its colour on its own; resending now and then recovers it
// after a glitch (a pixel that came up after the Pico, or a noisy edge).
#define LED_REFRESH_MS 500

static status_rgb_t shown;
static bool shown_valid;
static uint32_t next_refresh_ms;

static void send(status_rgb_t c) {
    uint32_t grb = ((uint32_t)c.g << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.b << 8);
    pio_sm_put_blocking(LED_PIO, LED_SM, grb);
}

void status_led_init(void) {
    uint offset = pio_add_program(LED_PIO, &status_led_program);
    pio_sm_config c = status_led_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, PIN_STATUS_LED);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&c, clock_get_hz(clk_sys) / 8000000.0f);
    pio_gpio_init(LED_PIO, PIN_STATUS_LED);
    pio_sm_set_consecutive_pindirs(LED_PIO, LED_SM, PIN_STATUS_LED, 1, true);
    pio_sm_init(LED_PIO, LED_SM, offset, &c);
    pio_sm_set_enabled(LED_PIO, LED_SM, true);
}

void status_led_poll(uint32_t now_ms) {
    status_rgb_t want = status_led_pick(ecu_get_power(), ecu_tripped(), crank_get_rpm());
    bool changed = !shown_valid || want.r != shown.r || want.g != shown.g || want.b != shown.b;
    if (!changed && (int32_t)(now_ms - next_refresh_ms) < 0) return;
    send(want);
    shown = want;
    shown_valid = true;
    next_refresh_ms = now_ms + LED_REFRESH_MS;
}
