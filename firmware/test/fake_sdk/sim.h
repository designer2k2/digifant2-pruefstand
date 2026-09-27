// Minimal simulated Pico SDK for running knock.c on a PC: a settable clock,
// one hardware alarm, and a log of every SPI word written with its timestamp.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef unsigned int uint;
typedef uint64_t absolute_time_t;
typedef struct spi_inst spi_inst_t;
typedef struct i2c_inst i2c_inst_t;
#define spi0 ((spi_inst_t *)0)
#define i2c0 ((i2c_inst_t *)0)

#define GPIO_OUT 1
#define GPIO_FUNC_SPI 1
#define SPI_CPOL_1 1
#define SPI_CPHA_0 0
#define SPI_MSB_FIRST 1

typedef void (*hardware_alarm_callback_t)(uint alarm_num);

extern uint64_t sim_now_us;
extern bool sim_alarm_armed;
extern uint64_t sim_alarm_target;
extern hardware_alarm_callback_t sim_alarm_cb;

#define SIM_LOG_MAX 1024
extern uint64_t sim_log_t[SIM_LOG_MAX];
extern uint16_t sim_log_word[SIM_LOG_MAX];
extern size_t sim_log_n;

// I2C: every write is logged (last one kept); sim_i2c_fail makes them NACK.
extern bool sim_i2c_fail;
extern uint8_t sim_i2c_last[16];
extern size_t sim_i2c_last_len;
extern size_t sim_i2c_writes;
static inline int i2c_write_timeout_us(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src,
                                       size_t len, bool nostop, uint timeout_us) {
    (void)i2c; (void)addr; (void)nostop; (void)timeout_us;
    sim_i2c_writes++;
    if (sim_i2c_fail) return -1;  // PICO_ERROR_GENERIC: address NACK
    sim_i2c_last_len = len < sizeof sim_i2c_last ? len : sizeof sim_i2c_last;
    for (size_t i = 0; i < sim_i2c_last_len; i++) sim_i2c_last[i] = src[i];
    return (int)len;
}

static inline uint32_t time_us_32(void) { return (uint32_t)sim_now_us; }
static inline uint64_t time_us_64(void) { return sim_now_us; }
static inline uint64_t to_us_since_boot(absolute_time_t t) { return t; }
static inline absolute_time_t make_timeout_time_us(uint64_t us) { return sim_now_us + us; }
static inline absolute_time_t delayed_by_us(absolute_time_t t, uint64_t us) { return t + us; }

static inline uint32_t save_and_disable_interrupts(void) { return 0; }
static inline void restore_interrupts(uint32_t s) { (void)s; }

static inline void gpio_init(uint p) { (void)p; }
static inline void gpio_put(uint p, bool v) { (void)p; (void)v; }
static inline void gpio_set_dir(uint p, int d) { (void)p; (void)d; }
static inline void gpio_set_function(uint p, int f) { (void)p; (void)f; }

static inline uint spi_init(spi_inst_t *s, uint baud) { (void)s; return baud; }
static inline void spi_set_format(spi_inst_t *s, uint bits, int cpol, int cpha, int order) {
    (void)s; (void)bits; (void)cpol; (void)cpha; (void)order;
}
static inline int spi_write16_blocking(spi_inst_t *s, const uint16_t *src, size_t len) {
    (void)s;
    for (size_t i = 0; i < len; i++) {
        if (sim_log_n < SIM_LOG_MAX) {
            sim_log_t[sim_log_n] = sim_now_us;
            sim_log_word[sim_log_n++] = src[i];
        }
    }
    return (int)len;
}

static inline int hardware_alarm_claim_unused(bool required) { (void)required; return 0; }
static inline void hardware_alarm_set_callback(uint n, hardware_alarm_callback_t cb) {
    (void)n;
    sim_alarm_cb = cb;
}
// Same contract as the SDK: returns true (and does not arm) if t has passed.
// Unlike real hardware, a callback only ever runs when the test calls it, so
// tests invoke sim_alarm_cb directly to model a stale, latched alarm IRQ.
static inline bool hardware_alarm_set_target(uint n, absolute_time_t t) {
    (void)n;
    if (t <= sim_now_us) {
        sim_alarm_armed = false;
        return true;
    }
    sim_alarm_armed = true;
    sim_alarm_target = t;
    return false;
}
static inline void hardware_alarm_cancel(uint n) {
    (void)n;
    sim_alarm_armed = false;
}
