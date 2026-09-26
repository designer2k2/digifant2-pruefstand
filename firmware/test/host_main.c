// Runs the command parser on a PC: one command per stdin line, replies on stdout.
#include <stdio.h>
#include <string.h>

#include "cmd.h"

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char buf[256];
    while (fgets(buf, sizeof buf, stdin)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        if (buf[0] == '\0') continue;
        buf[CMD_LINE_MAX - 1] = '\0';
        cmd_process_line(buf);
    }
    return 0;
}
