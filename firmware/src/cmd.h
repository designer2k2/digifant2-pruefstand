#pragma once

#define FW_VERSION "0.9.0"
#define CMD_LINE_MAX 128

// Parses and executes one command line (modified in place), prints exactly one
// response line starting with "OK" or "ERR".
void cmd_process_line(char *line);

// Shown as boot=... in status: "power" (default) or "watchdog".
void cmd_set_boot_reason(const char *reason);
