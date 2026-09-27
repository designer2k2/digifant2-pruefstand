#include "capture.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "knock.h"
#include "pico/stdlib.h"
#include "pins.h"

static cap_state_t state;

// One handler for all three pins. GP2 is driven by the crank PIO, but its pad
// input still sees the level: GP2 rising = VW-18 falling, the angle reference.
static void edge_isr(uint gpio, uint32_t events) {
    uint32_t now = time_us_32();
    if (gpio == PIN_CRANK) {
        if (events & GPIO_IRQ_EDGE_RISE) {
            cap_on_ref(&state, now);
            knock_on_crank_ref(now, state.ref_period_us);
        }
        return;
    }
    cap_channel_t ch = gpio == PIN_IGN_CAPTURE ? CAP_IGN : CAP_INJ;
    if (events & GPIO_IRQ_EDGE_FALL) cap_on_edge(&state, ch, false, now);
    if (events & GPIO_IRQ_EDGE_RISE) cap_on_edge(&state, ch, true, now);
}

void capture_init(void) {
    const uint32_t both = GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE;
    gpio_set_irq_enabled_with_callback(PIN_IGN_CAPTURE, both, true, edge_isr);
    gpio_set_irq_enabled(PIN_INJ_CAPTURE, both, true);
    gpio_set_irq_enabled(PIN_CRANK, GPIO_IRQ_EDGE_RISE, true);
}

cap_result_t capture_get(cap_channel_t ch, uint32_t ppr) {
    uint32_t irq_state = save_and_disable_interrupts();
    cap_state_t snap = state;
    uint32_t now = time_us_32();
    restore_interrupts(irq_state);
    return cap_result(&snap, ch, now, ppr);
}

const char *capture_name(cap_channel_t ch) { return ch == CAP_IGN ? "ign" : "inj"; }
