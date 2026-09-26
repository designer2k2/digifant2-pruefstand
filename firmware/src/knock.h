#pragma once

#include <stdbool.h>
#include <stdint.h>

#define KNOCK_HZ_MAX 20000

void knock_init(void);
bool knock_set_hz(uint32_t hz);  // 0 = off
uint32_t knock_get_hz(void);
bool knock_is_implemented(void);
