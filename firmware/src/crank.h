#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CRANK_RPM_MAX 8000
#define CRANK_PPR_MAX 60

// Unconfirmed defaults: 4-cylinder distributor Hall sender (4 vanes at half
// crank speed = 2 pulses per crank rev), 50% duty. Adjustable via `crank`.
#define CRANK_PPR_DEFAULT 2
#define CRANK_DUTY_DEFAULT 50

void crank_init(void);

bool crank_set_rpm(uint32_t rpm);  // 0 = stopped, VW-18 held high
uint32_t crank_get_rpm(void);

bool crank_set_ppr(uint32_t ppr);  // pulses per crank revolution
uint32_t crank_get_ppr(void);

bool crank_set_duty(uint32_t pct);  // % of each period VW-18 is high, 1..99
uint32_t crank_get_duty(void);

