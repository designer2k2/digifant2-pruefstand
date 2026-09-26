// In-memory stand-in for board.c so the parser can run without a Pico.
#include <string.h>

#include "board.h"

static bool ecu, idle;
static bool sensor_connected[SENSOR_COUNT] = {true, true, true};
static const char *names[SENSOR_COUNT] = {"air", "water", "lambda"};

void board_init(void) {}

void board_set_ecu_power(bool on) { ecu = on; }
bool board_get_ecu_power(void) { return ecu; }

void board_set_idle_switch(bool closed) { idle = closed; }
bool board_get_idle_switch(void) { return idle; }

void board_set_sensor_connected(sensor_t s, bool c) { sensor_connected[s] = c; }
bool board_get_sensor_connected(sensor_t s) { return sensor_connected[s]; }

const char *board_sensor_name(sensor_t s) { return names[s]; }

bool board_sensor_from_name(const char *name, sensor_t *out) {
    for (int s = 0; s < SENSOR_COUNT; s++) {
        if (strcmp(name, names[s]) == 0) {
            *out = (sensor_t)s;
            return true;
        }
    }
    return false;
}

uint32_t board_read_afm_ref_mv(void) { return 5000; }
