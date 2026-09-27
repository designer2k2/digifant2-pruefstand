#include <stdio.h>

#include "board.h"
#include "capture.h"
#include "cmd.h"
#include "crank.h"
#include "dac.h"
#include "ecu.h"
#include "hardware/watchdog.h"
#include "knock.h"
#include "pico/stdlib.h"
#include "sense.h"
#include "ui.h"

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

// Longest main-loop stall is a USB host that stops reading: stdio_usb gives up
// after 500 ms once, then drops output. A reset leaves the ECU unpowered (R16).
#define WATCHDOG_MS 2000

int main(void) {
    cmd_set_boot_reason(watchdog_enable_caused_reboot() ? "watchdog" : "power");
    board_init();
    crank_init();
    dac_init();
    sense_init();
    knock_init();
    capture_init();
    ui_init();
    stdio_init_all();
    watchdog_enable(WATCHDOG_MS, true);

    while (true) {
        watchdog_update();
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        ecu_poll(now_ms);
        dac_poll(now_ms);
        poll_serial();
        ui_poll();
    }
}
