#include "board.h"

#include <string.h>

#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pins.h"

static const uint sensor_pins[SENSOR_COUNT] = {
    [SENSOR_AIR] = PIN_SENSOR_AIR,
    [SENSOR_WATER] = PIN_SENSOR_WATER,
    [SENSOR_LAMBDA] = PIN_SENSOR_LAMBDA,
};

static const char *sensor_names[SENSOR_COUNT] = {
    [SENSOR_AIR] = "air",
    [SENSOR_WATER] = "water",
    [SENSOR_LAMBDA] = "lambda",
};

// Level is set before the pin becomes an output so it never glitches.
static void output_init(uint pin, bool level) {
    gpio_init(pin);
    gpio_put(pin, level);
    gpio_set_dir(pin, GPIO_OUT);
}

// ECU outputs are pulled up externally (10k / 1k); the RP2040's default
// ~50k pull-down would drag the ignition input's high level to ~2.7 V.
static void capture_input_init(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_disable_pulls(pin);
}

// A Pico reset in the middle of a transfer can leave a chip holding SDA low
// until it gets the rest of its byte. Clock SCL until SDA is released, then
// send a STOP (I2C-bus spec UM10204, 3.1.16). Open-drain by switching the
// pin direction; the 4.7k bus pull-ups do the high level.
static void i2c_bus_recover(void) {
    gpio_init(PIN_I2C_SDA);
    gpio_init(PIN_I2C_SCL);
    gpio_put(PIN_I2C_SDA, 0);
    gpio_put(PIN_I2C_SCL, 0);
    for (int i = 0; i < 9 && !gpio_get(PIN_I2C_SDA); i++) {
        gpio_set_dir(PIN_I2C_SCL, GPIO_OUT);
        sleep_us(5);
        gpio_set_dir(PIN_I2C_SCL, GPIO_IN);
        sleep_us(5);
    }
    gpio_set_dir(PIN_I2C_SCL, GPIO_OUT);
    sleep_us(5);
    gpio_set_dir(PIN_I2C_SDA, GPIO_OUT);
    sleep_us(5);
    gpio_set_dir(PIN_I2C_SCL, GPIO_IN);
    sleep_us(5);
    gpio_set_dir(PIN_I2C_SDA, GPIO_IN);
    sleep_us(5);
}

static void button_init(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

void board_init(void) {
    output_init(PIN_ECU_POWER, false);
    output_init(PIN_IDLE_SW, false);
    output_init(PIN_CRANK, false);
    for (int s = 0; s < SENSOR_COUNT; s++) {
        output_init(sensor_pins[s], true);
    }

    capture_input_init(PIN_IGN_CAPTURE);
    capture_input_init(PIN_INJ_CAPTURE);

    button_init(PIN_BTN_MENU);
    button_init(PIN_BTN_MINUS);
    button_init(PIN_BTN_PLUS);

    i2c_bus_recover();
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);

    adc_init();
    adc_gpio_init(PIN_AFM_REF_ADC);
}

void board_set_ecu_power(bool on) { gpio_put(PIN_ECU_POWER, on); }
bool board_get_ecu_power(void) { return gpio_get_out_level(PIN_ECU_POWER); }

void board_set_idle_switch(bool closed) { gpio_put(PIN_IDLE_SW, closed); }
bool board_get_idle_switch(void) { return gpio_get_out_level(PIN_IDLE_SW); }

void board_set_sensor_connected(sensor_t s, bool connected) {
    gpio_put(sensor_pins[s], connected);
}

bool board_get_sensor_connected(sensor_t s) {
    return gpio_get_out_level(sensor_pins[s]);
}

const char *board_sensor_name(sensor_t s) { return sensor_names[s]; }

bool board_sensor_from_name(const char *name, sensor_t *out) {
    for (int s = 0; s < SENSOR_COUNT; s++) {
        if (strcmp(name, sensor_names[s]) == 0) {
            *out = (sensor_t)s;
            return true;
        }
    }
    return false;
}

uint32_t board_read_afm_ref_mv(void) {
    adc_select_input(AFM_REF_ADC_INPUT);
    uint32_t raw = adc_read();
    return raw * 3300u * AFM_REF_DIVIDER / 4095u;
}
