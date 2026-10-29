#pragma once

#include <stdint.h>

// Ring of completed ECU output pulses, filled from the capture interrupt and
// drained from the main loop (one producer, one consumer, so no locking: each
// side only writes its own index). When it is full new pulses are dropped and
// counted, so a slow reader loses the newest data, never the order.
#define PULSE_LOG_N 1024u  // power of two

typedef struct {
    uint32_t t_us;    // start of the low phase, low 32 bits of the microsecond clock
    uint32_t low_us;  // width of the low phase
} pulse_rec_t;

typedef struct {
    pulse_rec_t rec[PULSE_LOG_N];
    volatile uint32_t head;  // next slot to write, free-running
    volatile uint32_t tail;  // next slot to read, free-running
    volatile uint32_t dropped;
} pulse_log_t;

void pulse_log_clear(pulse_log_t *l);
// Interrupt side. False (and counted) when full.
int pulse_log_push(pulse_log_t *l, uint32_t t_us, uint32_t low_us);
// Main side: copies up to max oldest records out and removes them.
uint32_t pulse_log_pop(pulse_log_t *l, pulse_rec_t *out, uint32_t max);
uint32_t pulse_log_pending(const pulse_log_t *l);
