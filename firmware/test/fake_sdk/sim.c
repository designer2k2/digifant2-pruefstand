#include "sim.h"

uint64_t sim_now_us;
bool sim_alarm_armed;
uint64_t sim_alarm_target;
hardware_alarm_callback_t sim_alarm_cb;
uint64_t sim_log_t[SIM_LOG_MAX];
uint16_t sim_log_word[SIM_LOG_MAX];
size_t sim_log_n;
