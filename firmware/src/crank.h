#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CRANK_RPM_MAX 8000

void crank_init(void);
bool crank_set_rpm(uint32_t rpm);
uint32_t crank_get_rpm(void);
bool crank_is_implemented(void);
