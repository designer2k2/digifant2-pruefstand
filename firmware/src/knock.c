#include "knock.h"

#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pins.h"

#define KNOCK_SPI_BAUD (1000 * 1000)

static uint32_t hz_setpoint;

// FSYNC frames each 16-bit word; it's a plain GPIO since GP16 isn't SPI0's CSn.
static void write_words(const uint16_t *words, size_t n) {
    for (size_t i = 0; i < n; i++) {
        gpio_put(PIN_KNOCK_FSYNC, 0);
        spi_write16_blocking(KNOCK_SPI_PORT, &words[i], 1);
        gpio_put(PIN_KNOCK_FSYNC, 1);
    }
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

    knock_set_hz(0);
}

bool knock_set_hz(uint32_t hz) {
    uint16_t words[AD9833_SEQ_MAX];
    size_t n = ad9833_sequence(hz, KNOCK_MCLK_HZ, words);
    if (n == 0) return false;
    write_words(words, n);
    hz_setpoint = hz;
    return true;
}

uint32_t knock_get_hz(void) { return hz_setpoint; }
