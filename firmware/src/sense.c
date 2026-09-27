#include "sense.h"

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pins.h"

#define I2C_TIMEOUT_US 5000

typedef struct {
    uint8_t addr;
    float shunt_ohm;
    float sign;
    bool configured;
} ina_t;

// U3's IN+ is on the ECU side of RS2 (VW-23), so valve current reads negative;
// flip it so current through the valve is positive (DESIGN.md "idle_valve").
static ina_t ina_ecu = {I2C_ADDR_INA_ECU, 0.020f, 1.0f, false};     // RS1
static ina_t ina_valve = {I2C_ADDR_INA_VALVE, 0.033f, -1.0f, false};  // RS2

static bool read_reg(uint8_t addr, uint8_t reg, uint16_t *out) {
    uint8_t buf[2];
    if (i2c_write_timeout_us(I2C_PORT, addr, &reg, 1, true, I2C_TIMEOUT_US) != 1) return false;
    if (i2c_read_timeout_us(I2C_PORT, addr, buf, 2, false, I2C_TIMEOUT_US) != 2) return false;
    *out = (uint16_t)(buf[0] << 8 | buf[1]);
    return true;
}

static bool write_reg(uint8_t addr, uint8_t reg, uint16_t value) {
    uint8_t buf[3] = {reg, (uint8_t)(value >> 8), (uint8_t)value};
    return i2c_write_timeout_us(I2C_PORT, addr, buf, 3, false, I2C_TIMEOUT_US) == 3;
}

// Only configure a chip that identifies as an INA226.
static bool configure(ina_t *ina) {
    uint16_t id;
    ina->configured = read_reg(ina->addr, INA226_REG_MANUFACTURER_ID, &id) &&
                      id == INA226_MANUFACTURER_ID &&
                      write_reg(ina->addr, INA226_REG_CONFIG, INA226_CONFIG);
    return ina->configured;
}

static ina_reading_t read_ina(ina_t *ina) {
    ina_reading_t r = {0};
    if (!ina->configured && !configure(ina)) return r;

    uint16_t shunt, bus;
    if (!read_reg(ina->addr, INA226_REG_SHUNT, &shunt) ||
        !read_reg(ina->addr, INA226_REG_BUS, &bus)) {
        ina->configured = false;
        return r;
    }
    r.valid = true;
    r.bus_v = ina226_bus_volts(bus);
    r.current_a = ina->sign * ina226_shunt_volts(shunt) / ina->shunt_ohm;
    return r;
}

void sense_init(void) {
    configure(&ina_ecu);
    configure(&ina_valve);
}

ina_reading_t sense_read_ecu(void) { return read_ina(&ina_ecu); }

ina_reading_t sense_read_valve(void) { return read_ina(&ina_valve); }
