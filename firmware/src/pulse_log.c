#include "pulse_log.h"

#define MASK (PULSE_LOG_N - 1u)

void pulse_log_clear(pulse_log_t *l) {
    l->head = 0;
    l->tail = 0;
    l->dropped = 0;
}

int pulse_log_push(pulse_log_t *l, uint32_t t_us, uint32_t low_us) {
    if (l->head - l->tail >= PULSE_LOG_N) {
        l->dropped++;
        return 0;
    }
    l->rec[l->head & MASK] = (pulse_rec_t){t_us, low_us};
    l->head++;
    return 1;
}

uint32_t pulse_log_pop(pulse_log_t *l, pulse_rec_t *out, uint32_t max) {
    uint32_t n = l->head - l->tail;
    if (n > max) n = max;
    for (uint32_t i = 0; i < n; i++) out[i] = l->rec[(l->tail + i) & MASK];
    l->tail += n;
    return n;
}

uint32_t pulse_log_pending(const pulse_log_t *l) { return l->head - l->tail; }
