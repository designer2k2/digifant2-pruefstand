#include "cmd.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "crank.h"
#include "dac.h"
#include "knock.h"
#include "sense.h"

#define MAX_ARGS 4

typedef void (*cmd_handler_t)(int argc, char **argv);

typedef struct {
    const char *name;
    int argc;  // expected argument count after the command name
    cmd_handler_t handler;
    const char *usage;
} cmd_t;

static bool parse_uint(const char *s, uint32_t *out) {
    if (*s == '\0') return false;
    char *end;
    unsigned long v = strtoul(s, &end, 10);
    if (*end != '\0') return false;
    *out = (uint32_t)v;
    return true;
}

static bool parse_on_off(const char *s, bool *out) {
    if (strcmp(s, "on") == 0) { *out = true; return true; }
    if (strcmp(s, "off") == 0) { *out = false; return true; }
    return false;
}

static const char *on_off(bool v) { return v ? "on" : "off"; }

static void print_reading(const char *prefix, ina_reading_t r) {
    if (r.valid) {
        printf(" %s_v=%.3f %s_a=%.4f", prefix, r.bus_v, prefix, r.current_a);
    } else {
        printf(" %s_v=na %s_a=na", prefix, prefix);
    }
}

static void cmd_help(int argc, char **argv);

static void cmd_ping(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("OK pong fw=%s\n", FW_VERSION);
}

static void cmd_status(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("OK fw=%s ecu=%s idle=%s rpm=%lu crank_ppr=%lu crank_duty=%lu", FW_VERSION,
           on_off(board_get_ecu_power()), on_off(board_get_idle_switch()),
           (unsigned long)crank_get_rpm(), (unsigned long)crank_get_ppr(),
           (unsigned long)crank_get_duty());
    for (int s = 0; s < SENSOR_COUNT; s++) {
        printf(" sensor_%s=%s", board_sensor_name((sensor_t)s),
               board_get_sensor_connected((sensor_t)s) ? "conn" : "open");
    }
    for (int ch = 0; ch < DAC_COUNT; ch++) {
        printf(" dac_%s=%lu", dac_channel_name((dac_channel_t)ch),
               (unsigned long)dac_get_mv((dac_channel_t)ch));
    }
    printf(" knock_hz=%lu", (unsigned long)knock_get_hz());

    printf(" stubs=");
    bool first = true;
    const struct { const char *name; bool impl; } mods[] = {
        {"crank", crank_is_implemented()},
        {"dac", dac_is_implemented()},
        {"sense", sense_is_implemented()},
        {"knock", knock_is_implemented()},
    };
    for (size_t i = 0; i < sizeof mods / sizeof mods[0]; i++) {
        if (mods[i].impl) continue;
        printf("%s%s", first ? "" : ",", mods[i].name);
        first = false;
    }
    if (first) printf("none");
    printf("\n");
}

static void cmd_ecu(int argc, char **argv) {
    (void)argc;
    bool on;
    if (!parse_on_off(argv[1], &on)) { printf("ERR expected on|off\n"); return; }
    board_set_ecu_power(on);
    printf("OK ecu=%s\n", on_off(on));
}

static void cmd_idle(int argc, char **argv) {
    (void)argc;
    bool on;
    if (!parse_on_off(argv[1], &on)) { printf("ERR expected on|off\n"); return; }
    board_set_idle_switch(on);
    printf("OK idle=%s\n", on_off(on));
}

static void cmd_sensor(int argc, char **argv) {
    (void)argc;
    sensor_t s;
    if (!board_sensor_from_name(argv[1], &s)) {
        printf("ERR unknown sensor, expected air|water|lambda\n");
        return;
    }
    bool connected;
    if (strcmp(argv[2], "conn") == 0) connected = true;
    else if (strcmp(argv[2], "open") == 0) connected = false;
    else { printf("ERR expected conn|open\n"); return; }
    board_set_sensor_connected(s, connected);
    printf("OK sensor_%s=%s\n", board_sensor_name(s), connected ? "conn" : "open");
}

static void cmd_rpm(int argc, char **argv) {
    (void)argc;
    uint32_t rpm;
    if (!parse_uint(argv[1], &rpm) || !crank_set_rpm(rpm)) {
        printf("ERR rpm must be 0..%d\n", CRANK_RPM_MAX);
        return;
    }
    printf("OK rpm=%lu\n", (unsigned long)rpm);
}

static void cmd_crank(int argc, char **argv) {
    (void)argc;
    uint32_t v;
    bool is_ppr = strcmp(argv[1], "ppr") == 0;
    if (!is_ppr && strcmp(argv[1], "duty") != 0) {
        printf("ERR expected ppr|duty\n");
        return;
    }
    if (!parse_uint(argv[2], &v) || !(is_ppr ? crank_set_ppr(v) : crank_set_duty(v))) {
        if (is_ppr) printf("ERR ppr must be 1..%d\n", CRANK_PPR_MAX);
        else printf("ERR duty must be 1..99\n");
        return;
    }
    printf("OK crank_%s=%lu\n", argv[1], (unsigned long)v);
}

static void cmd_dac(int argc, char **argv) {
    (void)argc;
    dac_channel_t ch;
    if (!dac_channel_from_name(argv[1], &ch)) {
        printf("ERR unknown channel, expected air|water|afm|lambda\n");
        return;
    }
    uint32_t mv;
    if (!parse_uint(argv[2], &mv) || !dac_set_mv(ch, mv)) {
        printf("ERR mv must be 0..%d\n", DAC_MV_MAX);
        return;
    }
    printf("OK dac_%s=%lu\n", dac_channel_name(ch), (unsigned long)mv);
}

static void cmd_knock(int argc, char **argv) {
    (void)argc;
    uint32_t hz = 0;
    bool ok = strcmp(argv[1], "off") == 0 || parse_uint(argv[1], &hz);
    if (!ok || !knock_set_hz(hz)) {
        printf("ERR expected off or 1..%d\n", KNOCK_HZ_MAX);
        return;
    }
    printf("OK knock_hz=%lu\n", (unsigned long)hz);
}

static void cmd_read(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("OK afm_ref_mv=%lu", (unsigned long)board_read_afm_ref_mv());
    print_reading("ecu", sense_read_ecu());
    print_reading("valve", sense_read_valve());
    printf("\n");
}

static const cmd_t commands[] = {
    {"ping",   0, cmd_ping,   "ping"},
    {"help",   0, cmd_help,   "help"},
    {"status", 0, cmd_status, "status"},
    {"read",   0, cmd_read,   "read"},
    {"ecu",    1, cmd_ecu,    "ecu on|off"},
    {"idle",   1, cmd_idle,   "idle on|off"},
    {"sensor", 2, cmd_sensor, "sensor air|water|lambda conn|open"},
    {"rpm",    1, cmd_rpm,    "rpm <0..8000>"},
    {"crank",  2, cmd_crank,  "crank ppr|duty <n>"},
    {"dac",    2, cmd_dac,    "dac air|water|afm|lambda <mV 0..3300>"},
    {"knock",  1, cmd_knock,  "knock off|<hz>"},
};

#define NUM_COMMANDS (sizeof commands / sizeof commands[0])

static void cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("OK ");
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        printf("%s%s", i ? "; " : "", commands[i].usage);
    }
    printf("\n");
}

void cmd_process_line(char *line) {
    for (char *p = line; *p; p++) *p = (char)tolower((unsigned char)*p);

    char *argv[MAX_ARGS + 1];
    int argc = 0;
    for (char *tok = strtok(line, " \t"); tok; tok = strtok(NULL, " \t")) {
        if (argc > MAX_ARGS) { printf("ERR too many arguments\n"); return; }
        argv[argc++] = tok;
    }
    if (argc == 0) return;

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(argv[0], commands[i].name) != 0) continue;
        if (argc - 1 != commands[i].argc) {
            printf("ERR usage: %s\n", commands[i].usage);
            return;
        }
        commands[i].handler(argc, argv);
        return;
    }
    printf("ERR unknown command '%s', try help\n", argv[0]);
}
