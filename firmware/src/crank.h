#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CRANK_RPM_MAX 8000
#define CRANK_PPR_MAX 60

// Defaults per the user's own distributor HiL notes ("DIZZY_FOUR_CYLINDER":
// 30 Hz square wave, 50% duty, at 900 rpm -> 2 pulses per crank rev).
// Still worth a scope check on the real bench (see BRINGUP.md); adjustable
// via `crank` either way.
#define CRANK_PPR_DEFAULT 2
#define CRANK_DUTY_DEFAULT 50

void crank_init(void);

bool crank_set_rpm(uint32_t rpm);  // 0 = stopped, VW-18 held high
uint32_t crank_get_rpm(void);

bool crank_set_ppr(uint32_t ppr);  // pulses per crank revolution
uint32_t crank_get_ppr(void);

bool crank_set_duty(uint32_t pct);  // % of each period VW-18 is high, 1..99
uint32_t crank_get_duty(void);

