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
#include "wave.h"

#define MAX_ARGS 6

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

// "1", "1.5", "0.03" -> value * 1000. Up to three decimals; no sign.
static bool parse_milli(const char *s, uint32_t *out) {
    uint64_t whole = 0, frac = 0;
    int digits = 0;
    bool dot = false, any = false;
    for (; *s; s++) {
        if (*s == '.') {
            if (dot) return false;
            dot = true;
            continue;
        }
        if (!isdigit((unsigned char)*s)) return false;
        any = true;
        unsigned d = (unsigned)(*s - '0');
        if (!dot) {
            whole = whole * 10 + d;
            if (whole > UINT32_MAX / 1000) return false;
        } else {
            if (digits == 3) return false;
            frac = frac * 10 + d;
            digits++;
        }
    }
    if (!any) return false;
    for (; digits < 3; digits++) frac *= 10;
    *out = (uint32_t)(whole * 1000 + frac);
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
    for (int ch = 0; ch < DAC_COUNT; ch++) {
        if (wave_active((dac_channel_t)ch)) {
            printf(" wave_%s=%s", dac_channel_name((dac_channel_t)ch),
                   wave_shape_name(wave_get((dac_channel_t)ch).shape));
        }
    }
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
    wave_stop(ch);  // an explicit value ends any wave on that channel
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


static void print_milli(uint32_t v) {
    printf("%lu.%03lu", (unsigned long)(v / 1000), (unsigned long)(v % 1000));
}

static void print_wave_detail(dac_channel_t ch, const wave_cfg_t *c) {
    const char *n = dac_channel_name(ch);
    switch (c->shape) {
    case WAVE_SINE:
    case WAVE_TRIANGLE:
    case WAVE_SQUARE:
        printf(" %s_freq_hz=", n);
        print_milli(c->freq_mhz);
        printf(" %s_amp_mv=%lu %s_offset_mv=%lu", n, (unsigned long)c->amp, n, (unsigned long)c->offset);
        if (c->shape == WAVE_SQUARE) printf(" %s_duty=%lu", n, (unsigned long)c->duty_pct);
        break;
    case WAVE_RAMP:
        printf(" %s_from_mv=%lu %s_to_mv=%lu %s_seconds=", n, (unsigned long)c->from_mv, n,
               (unsigned long)c->to_mv, n);
        print_milli(c->ramp_ms);
        break;
    case WAVE_LAMBDA:
        printf(" %s_freq_hz=", n);
        print_milli(c->freq_mhz);
        printf(" %s_amp=", n);
        print_milli(c->amp);
        printf(" %s_offset=", n);
        print_milli(c->offset);
        break;
    case WAVE_LOOP:
        printf(" %s_lambda0=", n);
        print_milli(c->offset);
        printf(" %s_delay_ms=%lu %s_ref_us=%lu", n, (unsigned long)c->delay_ms, n, (unsigned long)c->ref_us);
        break;
    default: break;
    }
}

#define WAVE_USAGE \
    "ERR usage: wave [off [<ch>]] | <ch> sine|tri|square <Hz> <amp_mV> <offset_mV> [duty%%] | " \
    "<ch> ramp <from_mV> <to_mV> <seconds> | lambda lam <Hz> <amp> <offset> | " \
    "lambda loop <lambda0> <delay_ms> [ref_us]\n"

static void cmd_wave(int argc, char **argv) {
    if (argc == 1) {
        printf("OK");
        for (int ch = 0; ch < DAC_COUNT; ch++) {
            wave_cfg_t c = wave_get((dac_channel_t)ch);
            printf(" wave_%s=%s", dac_channel_name((dac_channel_t)ch), wave_shape_name(c.shape));
            print_wave_detail((dac_channel_t)ch, &c);
        }
        printf("\n");
        return;
    }
    if (strcmp(argv[1], "off") == 0) {
        if (argc == 2) {
            wave_stop_all();
            printf("OK wave=off\n");
            return;
        }
        dac_channel_t och;
        if (argc != 3 || !dac_channel_from_name(argv[2], &och)) { printf(WAVE_USAGE); return; }
        wave_stop(och);
        printf("OK wave_%s=off\n", dac_channel_name(och));
        return;
    }
    dac_channel_t ch;
    if (argc < 5 || !dac_channel_from_name(argv[1], &ch)) { printf(WAVE_USAGE); return; }
    const char *shape = argv[2];
    wave_cfg_t c = {0};
    c.duty_pct = 50;
    bool ok = false;
    if (strcmp(shape, "sine") == 0 || strcmp(shape, "tri") == 0 || strcmp(shape, "triangle") == 0 ||
        strcmp(shape, "square") == 0) {
        c.shape = shape[0] == 's' && shape[1] == 'i' ? WAVE_SINE : shape[0] == 't' ? WAVE_TRIANGLE : WAVE_SQUARE;
        ok = argc >= 6 && argc <= (c.shape == WAVE_SQUARE ? 7 : 6) && parse_milli(argv[3], &c.freq_mhz) &&
             parse_uint(argv[4], &c.amp) && parse_uint(argv[5], &c.offset) &&
             (argc < 7 || parse_uint(argv[6], &c.duty_pct));
    } else if (strcmp(shape, "ramp") == 0) {
        c.shape = WAVE_RAMP;
        ok = argc == 6 && parse_uint(argv[3], &c.from_mv) && parse_uint(argv[4], &c.to_mv) &&
             parse_milli(argv[5], &c.ramp_ms);
    } else if (strcmp(shape, "lam") == 0) {
        c.shape = WAVE_LAMBDA;
        ok = argc == 6 && parse_milli(argv[3], &c.freq_mhz) && parse_milli(argv[4], &c.amp) &&
             parse_milli(argv[5], &c.offset);
    } else if (strcmp(shape, "loop") == 0) {
        c.shape = WAVE_LOOP;
        ok = (argc == 5 || argc == 6) && parse_milli(argv[3], &c.offset) &&
             parse_uint(argv[4], &c.delay_ms) && (argc < 6 || parse_uint(argv[5], &c.ref_us));
    }
    if (!ok) { printf(WAVE_USAGE); return; }
    const char *why = wave_start(ch, &c);
    if (why) { printf("ERR %s\n", why); return; }
    printf("OK wave_%s=%s t0_us=%lu\n", dac_channel_name(ch), wave_shape_name(c.shape),
           (unsigned long)wave_t0_us(ch));
}

#define LOG_READ_MAX 200

static void cmd_log(int argc, char **argv) {
    if (strcmp(argv[1], "start") == 0 && argc == 2) {
        capture_log_start();
        printf("OK log=on\n");
    } else if (strcmp(argv[1], "stop") == 0 && argc == 2) {
        capture_log_stop();
        printf("OK log=off\n");
    } else if (strcmp(argv[1], "status") == 0 && argc == 2) {
        printf("OK log=%s", on_off(capture_log_running()));
        for (int ch = 0; ch < CAP_COUNT; ch++) {
            const char *n = capture_name((cap_channel_t)ch);
            printf(" %s_pending=%lu %s_dropped=%lu", n, (unsigned long)capture_log_pending((cap_channel_t)ch),
                   n, (unsigned long)capture_log_dropped((cap_channel_t)ch));
        }
        printf("\n");
    } else if (strcmp(argv[1], "read") == 0 && (argc == 3 || argc == 4)) {
        cap_channel_t ch;
        if (strcmp(argv[2], "ign") == 0) ch = CAP_IGN;
        else if (strcmp(argv[2], "inj") == 0) ch = CAP_INJ;
        else { printf("ERR expected ign|inj\n"); return; }
        uint32_t n = 100;
        if (argc == 4 && (!parse_uint(argv[3], &n) || n == 0 || n > LOG_READ_MAX)) {
            printf("ERR n must be 1..%d\n", LOG_READ_MAX);
            return;
        }
        pulse_rec_t buf[LOG_READ_MAX];
        uint32_t got = capture_log_read(ch, buf, n);
        printf("OK log_%s_n=%lu pending=%lu dropped=%lu lines=%lu\n", capture_name(ch), (unsigned long)got,
               (unsigned long)capture_log_pending(ch), (unsigned long)capture_log_dropped(ch),
               (unsigned long)got);
        for (uint32_t i = 0; i < got; i++) {
            printf("%lu %lu\n", (unsigned long)buf[i].t_us, (unsigned long)buf[i].low_us);
        }
    } else {
        printf("ERR usage: log start|stop|status | log read ign|inj [n]\n");
    }
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
    {"wave",    0, 6, cmd_wave,     "wave [off [ch]|<ch> sine|tri|square|ramp|lam|loop ...]"},
    {"log",     1, 3, cmd_log,      "log start|stop|status|read ign|inj [n]"},
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
