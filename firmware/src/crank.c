#include "crank.h"

#include "crank.pio.h"
#include "crank_timing.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/sync.h"
#include "pins.h"

#define CRANK_PIO pio0
#define CRANK_SM 0
#define CRANK_PIO_IRQ PIO0_IRQ_0
#define CRANK_IRQ_SOURCE pis_sm0_tx_fifo_not_full

static uint32_t rpm_setpoint;
static uint32_t ppr = CRANK_PPR_DEFAULT;
static uint32_t duty = CRANK_DUTY_DEFAULT;
static uint prog_offset;

static volatile uint32_t vw_low_loops, vw_high_loops;
static volatile bool next_is_vw_low;

// The program consumes words strictly alternating (GP2 high = VW-18 low first),
// so the ISR tracks which phase the next word is for.
static void refill_isr(void) {
    while (!pio_sm_is_tx_fifo_full(CRANK_PIO, CRANK_SM)) {
        pio_sm_put(CRANK_PIO, CRANK_SM, next_is_vw_low ? vw_low_loops : vw_high_loops);
        next_is_vw_low = !next_is_vw_low;
    }
}

static void stop(void) {
    pio_set_irq0_source_enabled(CRANK_PIO, CRANK_IRQ_SOURCE, false);
    pio_sm_set_enabled(CRANK_PIO, CRANK_SM, false);
    pio_sm_exec(CRANK_PIO, CRANK_SM, pio_encode_set(pio_pins, 0));
}

static void start(void) {
    pio_sm_clear_fifos(CRANK_PIO, CRANK_SM);
    pio_sm_restart(CRANK_PIO, CRANK_SM);
    pio_sm_exec(CRANK_PIO, CRANK_SM, pio_encode_jmp(prog_offset));
    next_is_vw_low = true;
    pio_set_irq0_source_enabled(CRANK_PIO, CRANK_IRQ_SOURCE, true);
    pio_sm_set_enabled(CRANK_PIO, CRANK_SM, true);
}

// Validates the settings and, if the wave is running, swaps them in. New timing
// takes effect once the already-queued phases (up to ~2.5 periods) have played out.
static bool apply(uint32_t new_rpm, uint32_t new_ppr, uint32_t new_duty) {
    crank_phases_t p = {0};
    if (new_rpm > 0 &&
        !crank_compute_phases(new_rpm, new_ppr, new_duty, clock_get_hz(clk_sys), &p)) {
        return false;
    }

    bool was_running = rpm_setpoint > 0;
    rpm_setpoint = new_rpm;
    ppr = new_ppr;
    duty = new_duty;

    if (new_rpm == 0) {
        if (was_running) stop();
        return true;
    }

    uint32_t irq_state = save_and_disable_interrupts();
    vw_low_loops = p.vw_low_loops;
    vw_high_loops = p.vw_high_loops;
    restore_interrupts(irq_state);

    if (!was_running) start();
    return true;
}

void crank_init(void) {
    prog_offset = pio_add_program(CRANK_PIO, &crank_program);
    pio_sm_config c = crank_program_get_default_config(prog_offset);
    sm_config_set_set_pins(&c, PIN_CRANK, 1);
    pio_sm_init(CRANK_PIO, CRANK_SM, prog_offset, &c);

    // Drive the PIO output low before handing it the pin, so Q1's gate never floats.
    pio_sm_set_pins_with_mask(CRANK_PIO, CRANK_SM, 0, 1u << PIN_CRANK);
    pio_sm_set_pindirs_with_mask(CRANK_PIO, CRANK_SM, 1u << PIN_CRANK, 1u << PIN_CRANK);
    pio_gpio_init(CRANK_PIO, PIN_CRANK);

    irq_set_exclusive_handler(CRANK_PIO_IRQ, refill_isr);
    irq_set_enabled(CRANK_PIO_IRQ, true);
}

bool crank_set_rpm(uint32_t rpm) {
    if (rpm > CRANK_RPM_MAX) return false;
    return apply(rpm, ppr, duty);
}

uint32_t crank_get_rpm(void) { return rpm_setpoint; }

bool crank_set_ppr(uint32_t new_ppr) {
    if (new_ppr == 0 || new_ppr > CRANK_PPR_MAX) return false;
    return apply(rpm_setpoint, new_ppr, duty);
}

uint32_t crank_get_ppr(void) { return ppr; }

bool crank_set_duty(uint32_t pct) {
    if (pct == 0 || pct >= 100) return false;
    return apply(rpm_setpoint, ppr, pct);
}

uint32_t crank_get_duty(void) { return duty; }

bool crank_is_implemented(void) { return true; }
