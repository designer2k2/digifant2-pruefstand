// The pulse ring (pulse_log.c): order, wrap-around, overflow accounting.
#include <stdio.h>
#include <string.h>

#include "pulse_log.h"

static int failures;

static void check(const char *what, int ok) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static pulse_log_t log_;

int main(void) {
    pulse_log_clear(&log_);
    check("empty at start", pulse_log_pending(&log_) == 0);

    for (uint32_t i = 0; i < 10; i++) pulse_log_push(&log_, 1000 + i, 2600 + i);
    check("10 pushed, 10 pending", pulse_log_pending(&log_) == 10);
    pulse_rec_t out[16];
    uint32_t n = pulse_log_pop(&log_, out, 4);
    check("pop returns the oldest first", n == 4 && out[0].t_us == 1000 && out[3].low_us == 2603);
    check("pop removes what it returned", pulse_log_pending(&log_) == 6);
    n = pulse_log_pop(&log_, out, 16);
    check("pop returns at most what is there", n == 6 && out[0].t_us == 1004 && out[5].t_us == 1009);

    // Fill it completely, then one more.
    pulse_log_clear(&log_);
    for (uint32_t i = 0; i < PULSE_LOG_N; i++) pulse_log_push(&log_, i, i);
    check("holds exactly PULSE_LOG_N records", pulse_log_pending(&log_) == PULSE_LOG_N && log_.dropped == 0);
    int pushed = pulse_log_push(&log_, 9999, 9999);
    check("a full ring refuses and counts the drop", !pushed && log_.dropped == 1 &&
                                                       pulse_log_pending(&log_) == PULSE_LOG_N);
    n = pulse_log_pop(&log_, out, 1);
    check("the oldest record survives an overflow", n == 1 && out[0].t_us == 0);
    check("space frees up after a pop", pulse_log_push(&log_, 777, 777) == 1);

    // Free-running indexes wrapping past 2^32.
    pulse_log_clear(&log_);
    log_.head = log_.tail = 0xFFFFFFF0u;
    for (uint32_t i = 0; i < 40; i++) pulse_log_push(&log_, i, i * 2);
    n = 0;
    pulse_rec_t big[64];
    n = pulse_log_pop(&log_, big, 64);
    int ordered = n == 40;
    for (uint32_t i = 0; i < n; i++) ordered &= big[i].t_us == i && big[i].low_us == i * 2;
    check("indexes wrap past 2^32 without losing order", ordered);

    printf("%s\n", failures ? "FAILED" : "all ok");
    return failures ? 1 : 0;
}
