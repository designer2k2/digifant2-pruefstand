#pragma once

#include <stdbool.h>

#define FW_VERSION "0.10.1"
#define CMD_LINE_MAX 128

// Parses and executes one command line (modified in place), prints exactly one
// response line starting with "OK" or "ERR". `log read` follows its line with
// data lines, which go out a few per cmd_poll() call so the main loop keeps
// running; don't process another line while cmd_busy().
void cmd_process_line(char *line);
bool cmd_busy(void);
void cmd_poll(void);  // main loop

// Shown as boot=... in status: "power" (default) or "watchdog".
void cmd_set_boot_reason(const char *reason);
