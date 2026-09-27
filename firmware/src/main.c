#include <stdio.h>

#include "board.h"
#include "capture.h"
#include "cmd.h"
#include "crank.h"
#include "dac.h"
#include "knock.h"
#include "pico/stdlib.h"
#include "sense.h"

static void poll_serial(void) {
    static char line[CMD_LINE_MAX];
    static size_t len;
    static bool overflow;

    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\r' || c == '\n') {
            if (overflow) {
                printf("ERR line too long (max %d)\n", CMD_LINE_MAX - 1);
            } else if (len > 0) {
                line[len] = '\0';
                cmd_process_line(line);
            }
            len = 0;
            overflow = false;
        } else if (len < CMD_LINE_MAX - 1) {
            line[len++] = (char)c;
        } else {
            overflow = true;
        }
    }
}

int main(void) {
    board_init();
    crank_init();
    dac_init();
    sense_init();
    knock_init();
    capture_init();
    stdio_init_all();

    while (true) {
        poll_serial();
        tight_loop_contents();
    }
}
