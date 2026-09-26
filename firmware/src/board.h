#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum { SENSOR_AIR, SENSOR_WATER, SENSOR_LAMBDA, SENSOR_COUNT } sensor_t;

void board_init(void);

void board_set_ecu_power(bool on);
bool board_get_ecu_power(void);

void board_set_idle_switch(bool closed);
bool board_get_idle_switch(void);

void board_set_sensor_connected(sensor_t s, bool connected);
bool board_get_sensor_connected(sensor_t s);
const char *board_sensor_name(sensor_t s);
bool board_sensor_from_name(const char *name, sensor_t *out);

uint32_t board_read_afm_ref_mv(void);
