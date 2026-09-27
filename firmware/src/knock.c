#include "knock.h"

#include "crank.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"
#include "pins.h"

// Fast enough that a burst start/stop word costs ~2 us inside the alarm IRQ.
#define KNOCK_SPI_BAUD (8 * 1000 * 1000)

static const uint16_t WORD_RESET = AD9833_B28 | AD9833_RESET;
static const uint16_t WORD_RUN = AD9833_B28;

static uint32_t hz_setpoint;
static knock_burst_t burst;
static uint32_t ref_index;
static uint alarm_num;
static bool stop_pending;          // the armed alarm is the burst's stop, not its start
static absolute_time_t stop_at;

// FSYNC frames each 16-bit word; it's a plain GPIO since GP16 isn't SPI0's CSn.
// Callers outside the alarm IRQ must hold interrupts off (see locked_write).
static void write_words(const uint16_t *words, size_t n) {
    for (size_t i = 0; i < n; i++) {
        gpio_put(PIN_KNOCK_FSYNC, 0);
        spi_write16_blocking(KNOCK_SPI_PORT, &words[i], 1);
        gpio_put(PIN_KNOCK_FSYNC, 1);
    }
}

static void write_word(uint16_t w) { write_words(&w, 1); }

static void cancel_burst(void) {
    hardware_alarm_cancel(alarm_num);
    stop_pending = false;
}

// Alarm fires twice per burst: first to start the tone, then to stop it.
static void alarm_isr(uint num) {
    (void)num;
    if (!stop_pending) {
        write_word(WORD_RUN);
        stop_pending = true;
        if (!hardware_alarm_set_target(alarm_num, stop_at)) return;
        // Stop time already passed (burst shorter than our latency): stop now.
    }
    write_word(WORD_RESET);
    stop_pending = false;
}

void knock_on_crank_ref(uint32_t now_us, uint32_t ref_period_us) {
    if (!burst.enabled || hz_setpoint == 0) return;
    if (stop_pending) write_word(WORD_RESET);  // never let a burst run into the next one
    cancel_burst();

    uint32_t start_us, len_us;
    if (!knock_burst_timing(&burst, ref_index++, ref_period_us, crank_get_ppr(), &start_us,
                            &len_us)) {
        return;
    }
    uint32_t elapsed = time_us_32() - now_us;
    uint32_t wait = start_us > elapsed ? start_us - elapsed : 0;
    absolute_time_t start_at = make_timeout_time_us(wait);
    stop_at = delayed_by_us(start_at, len_us);
    if (hardware_alarm_set_target(alarm_num, start_at)) alarm_isr(alarm_num);
}

void knock_init(void) {
    gpio_init(PIN_KNOCK_FSYNC);
    gpio_put(PIN_KNOCK_FSYNC, 1);
    gpio_set_dir(PIN_KNOCK_FSYNC, GPIO_OUT);

    // Mode 2: SCLK idles high, data clocked in on the falling edge.
    spi_init(KNOCK_SPI_PORT, KNOCK_SPI_BAUD);
    spi_set_format(KNOCK_SPI_PORT, 16, SPI_CPOL_1, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_KNOCK_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_KNOCK_SDATA, GPIO_FUNC_SPI);

    alarm_num = (uint)hardware_alarm_claim_unused(true);
    hardware_alarm_set_callback(alarm_num, alarm_isr);

    write_word(WORD_RESET);
}

// Brings the chip in line with hz_setpoint and the burst mode. In burst mode
// the frequency is loaded but the chip stays in reset until a burst starts.
static void apply_output(void) {
    cancel_burst();
    uint16_t words[AD9833_SEQ_MAX];
    size_t n = ad9833_sequence(hz_setpoint, KNOCK_MCLK_HZ, words);
    if (hz_setpoint > 0 && burst.enabled) n--;  // drop the final "release reset"
    write_words(words, n);
}

bool knock_set_hz(uint32_t hz) {
    uint16_t unused[AD9833_SEQ_MAX];
    if (ad9833_sequence(hz, KNOCK_MCLK_HZ, unused) == 0) return false;
    uint32_t irq = save_and_disable_interrupts();
    hz_setpoint = hz;
    apply_output();
    restore_interrupts(irq);
    return true;
}

uint32_t knock_get_hz(void) { return hz_setpoint; }

bool knock_set_burst(uint32_t start_deg, uint32_t len_deg, uint32_t every) {
    if (!knock_burst_valid(start_deg, len_deg, every, crank_get_ppr())) return false;
    uint32_t irq = save_and_disable_interrupts();
    burst = (knock_burst_t){true, start_deg, len_deg, every};
    ref_index = 0;
    apply_output();
    restore_interrupts(irq);
    return true;
}

void knock_burst_off(void) {
    uint32_t irq = save_and_disable_interrupts();
    burst.enabled = false;
    apply_output();
    restore_interrupts(irq);
}

knock_burst_t knock_get_burst(void) { return burst; }
