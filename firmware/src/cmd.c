#include "cmd.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "capture.h"
#include "crank.h"
#include "dac.h"
#include "ecu.h"
#include "knock.h"
#include "sense.h"

#define MAX_ARGS 4

typedef void (*cmd_handler_t)(int argc, char **argv);

typedef struct {
    const char *name;
    int argc_min, argc_max;  // argument count after the command name
    cmd_handler_t handler;
    const char *usage;
} cmd_t;

static const char *boot_reason = "power";

void cmd_set_boot_reason(const char *reason) { boot_reason = reason; }

// Decimal digits only: strtoul would also take a sign ("-1" wraps to a huge
// value) and saturate or truncate on overflow depending on the platform.
static bool parse_uint(const char *s, uint32_t *out) {
    if (*s == '\0') return false;
    uint64_t v = 0;
    for (; *s; s++) {
        if (!isdigit((unsigned char)*s)) return false;
        v = v * 10 + (uint64_t)(*s - '0');
        if (v > UINT32_MAX) return false;
    }
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
    printf("OK fw=%s boot=%s ecu=%s ecu_fault=%s ecu_trip_ma=%lu idle=%s rpm=%lu crank_ppr=%lu "
           "crank_duty=%lu",
           FW_VERSION, boot_reason, on_off(ecu_get_power()),
           ecu_tripped() ? "overcurrent" : "none", (unsigned long)ecu_get_trip_ma(),
           on_off(board_get_idle_switch()),
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
    knock_burst_t b = knock_get_burst();
    printf(" dac_i2c=%s knock_hz=%lu burst=%s burst_start_deg=%lu burst_len_deg=%lu burst_every=%lu",
           dac_i2c_ok() ? "ok" : "err", (unsigned long)knock_get_hz(), on_off(b.enabled),
           (unsigned long)b.start_deg, (unsigned long)b.len_deg, (unsigned long)b.every);
    printf("\n");
}

static void cmd_ecu(int argc, char **argv) {
    if (argc == 3) {
        uint32_t ma;
        if (strcmp(argv[1], "trip") != 0) { printf("ERR usage: ecu on|off|reset|trip <mA>\n"); return; }
        if (!parse_uint(argv[2], &ma) || !ecu_set_trip_ma(ma)) {
            printf("ERR trip must be %d..%d mA\n", ECU_TRIP_MA_MIN, ECU_TRIP_MA_MAX);
            return;
        }
        printf("OK ecu_trip_ma=%lu\n", (unsigned long)ma);
        return;
    }
    if (strcmp(argv[1], "reset") == 0) {
        ecu_reset_fault();
        printf("OK ecu_fault=none\n");
        return;
    }
    bool on;
    if (!parse_on_off(argv[1], &on)) { printf("ERR usage: ecu on|off|reset|trip <mA>\n"); return; }
    if (!ecu_set_power(on)) {
        printf("ERR ecu tripped at %lu mA, send 'ecu reset' first\n",
               (unsigned long)ecu_trip_reading_ma());
        return;
    }
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
    if (!parse_uint(argv[1], &rpm) || rpm > CRANK_RPM_MAX) {
        printf("ERR rpm must be 0..%d\n", CRANK_RPM_MAX);
        return;
    }
    if (!crank_set_rpm(rpm)) {
        printf("ERR rpm %lu not possible at crank_ppr=%lu crank_duty=%lu\n", (unsigned long)rpm,
               (unsigned long)crank_get_ppr(), (unsigned long)crank_get_duty());
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
    if (!parse_uint(argv[2], &v) || (is_ppr ? v == 0 || v > CRANK_PPR_MAX : v == 0 || v > 99)) {
        if (is_ppr) printf("ERR ppr must be 1..%d\n", CRANK_PPR_MAX);
        else printf("ERR duty must be 1..99\n");
        return;
    }
    // A running burst must still fit inside one reference period.
    knock_burst_t b = knock_get_burst();
    if (is_ppr && b.enabled && !knock_burst_valid(b.start_deg, b.len_deg, b.every, v)) {
        printf("ERR burst %lu+%lu deg doesn't fit in %lu deg at ppr %lu, change or stop it first\n",
               (unsigned long)b.start_deg, (unsigned long)b.len_deg, (unsigned long)(360 / v),
               (unsigned long)v);
        return;
    }
    if (!(is_ppr ? crank_set_ppr(v) : crank_set_duty(v))) {
        printf("ERR not possible at rpm=%lu\n", (unsigned long)crank_get_rpm());
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
    dac_result_t r = parse_uint(argv[2], &mv) ? dac_set_mv(ch, mv) : DAC_OUT_OF_RANGE;
    if (r == DAC_OUT_OF_RANGE) {
        printf("ERR %s mv must be 0..%lu\n", dac_channel_name(ch),
               (unsigned long)dac_max_mv(ch));
        return;
    }
    if (r == DAC_I2C_ERROR) {
        printf("ERR dac not responding on i2c\n");
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

static void cmd_burst(int argc, char **argv) {
    if (argc == 2) {
        if (strcmp(argv[1], "off") != 0) {
            printf("ERR usage: burst off|<start_deg> <len_deg> [every]\n");
            return;
        }
        knock_burst_off();
        printf("OK burst=off\n");
        return;
    }
    uint32_t start, len, every = 1;
    if (!parse_uint(argv[1], &start) || !parse_uint(argv[2], &len) ||
        (argc == 4 && !parse_uint(argv[3], &every)) || !knock_set_burst(start, len, every)) {
        printf("ERR need len>0, every>0 and start+len < %lu deg (360/ppr)\n",
               (unsigned long)(360 / crank_get_ppr()));
        return;
    }
    printf("OK burst=on burst_start_deg=%lu burst_len_deg=%lu burst_every=%lu\n",
           (unsigned long)start, (unsigned long)len, (unsigned long)every);
}

static void cmd_read(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("OK afm_ref_mv=%lu", (unsigned long)board_read_afm_ref_mv());
    print_reading("ecu", sense_read_ecu());
    print_reading("valve", sense_read_valve());
    printf("\n");
}

static void cmd_capture(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("OK");
    for (int ch = 0; ch < CAP_COUNT; ch++) {
        const char *n = capture_name((cap_channel_t)ch);
        cap_result_t r = capture_get((cap_channel_t)ch, crank_get_ppr());
        printf(" %s_n=%lu %s_glitch=%lu", n, (unsigned long)r.count, n, (unsigned long)r.glitches);
        if (r.valid && r.period_us) printf(" %s_period_us=%lu", n, (unsigned long)r.period_us);
        else printf(" %s_period_us=na", n);
        if (r.valid) printf(" %s_low_us=%lu", n, (unsigned long)r.low_us);
        else printf(" %s_low_us=na", n);
        if (r.have_angle) printf(" %s_fall_deg=%.1f %s_rise_deg=%.1f", n, r.fall_deg, n, r.rise_deg);
        else printf(" %s_fall_deg=na %s_rise_deg=na", n, n);
    }
    printf("\n");
}

static const cmd_t commands[] = {
    {"ping",    0, 0, cmd_ping,     "ping"},
    {"help",    0, 0, cmd_help,     "help"},
    {"status",  0, 0, cmd_status,   "status"},
    {"read",    0, 0, cmd_read,     "read"},
    {"capture", 0, 0, cmd_capture,  "capture"},
    {"ecu",     1, 2, cmd_ecu,      "ecu on|off|reset|trip <mA>"},
    {"idle",    1, 1, cmd_idle,     "idle on|off"},
    {"sensor",  2, 2, cmd_sensor,   "sensor air|water|lambda conn|open"},
    {"rpm",     1, 1, cmd_rpm,      "rpm <0..8000>"},
    {"crank",   2, 2, cmd_crank,    "crank ppr|duty <n>"},
    {"dac",     2, 2, cmd_dac,      "dac air|water|afm|lambda <mV>"},
    {"knock",   1, 1, cmd_knock,    "knock off|<hz>"},
    {"burst",   1, 3, cmd_burst,    "burst off|<start_deg> <len_deg> [every]"},
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
        if (argc - 1 < commands[i].argc_min || argc - 1 > commands[i].argc_max) {
            printf("ERR usage: %s\n", commands[i].usage);
            return;
        }
        commands[i].handler(argc, argv);
        return;
    }
    printf("ERR unknown command '%s', try help\n", argv[0]);
}
