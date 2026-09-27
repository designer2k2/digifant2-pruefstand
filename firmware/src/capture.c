#include "capture.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "knock.h"
#include "pico/stdlib.h"
#include "pins.h"

#define EDGES (GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE)

static cap_state_t state;

static uint32_t to_cap_events(uint32_t gpio_events) {
    return (gpio_events & GPIO_IRQ_EDGE_FALL ? CAP_EV_FALL : 0) |
           (gpio_events & GPIO_IRQ_EDGE_RISE ? CAP_EV_RISE : 0);
}

// Raw handler for all three pins, so every edge seen in one interrupt gets
// the same timestamp, taken first thing, instead of the SDK's per-pin
// callbacks each reading the clock in pin order. GP2 is driven by the crank
// PIO, but its pad input still sees the level: GP2 rising = VW-18 falling,
// the angle reference.
static void edge_isr(void) {
    uint64_t now = time_us_64();
    uint32_t ref_ev = gpio_get_irq_event_mask(PIN_CRANK) & GPIO_IRQ_EDGE_RISE;
    uint32_t ign_ev = gpio_get_irq_event_mask(PIN_IGN_CAPTURE) & EDGES;
    uint32_t inj_ev = gpio_get_irq_event_mask(PIN_INJ_CAPTURE) & EDGES;
    if (ref_ev) gpio_acknowledge_irq(PIN_CRANK, ref_ev);
    if (ign_ev) gpio_acknowledge_irq(PIN_IGN_CAPTURE, ign_ev);
    if (inj_ev) gpio_acknowledge_irq(PIN_INJ_CAPTURE, inj_ev);

    const uint32_t ev[CAP_COUNT] = {to_cap_events(ign_ev), to_cap_events(inj_ev)};
    const bool high[CAP_COUNT] = {gpio_get(PIN_IGN_CAPTURE), gpio_get(PIN_INJ_CAPTURE)};
    if (cap_on_irq(&state, now, ref_ev != 0, ev, high)) {
        knock_on_crank_ref((uint32_t)now, state.ref_period_us);
    }
}

void capture_init(void) {
    gpio_add_raw_irq_handler_masked((1u << PIN_IGN_CAPTURE) | (1u << PIN_INJ_CAPTURE) |
                                        (1u << PIN_CRANK),
                                    edge_isr);
    gpio_set_irq_enabled(PIN_IGN_CAPTURE, EDGES, true);
    gpio_set_irq_enabled(PIN_INJ_CAPTURE, EDGES, true);
    gpio_set_irq_enabled(PIN_CRANK, GPIO_IRQ_EDGE_RISE, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

cap_result_t capture_get(cap_channel_t ch, uint32_t ppr) {
    uint32_t irq_state = save_and_disable_interrupts();
    cap_state_t snap = state;
    uint64_t now = time_us_64();
    restore_interrupts(irq_state);
    return cap_result(&snap, ch, now, ppr);
}

const char *capture_name(cap_channel_t ch) { return ch == CAP_IGN ? "ign" : "inj"; }
