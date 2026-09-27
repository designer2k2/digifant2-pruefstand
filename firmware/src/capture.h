#pragma once

#include <stdbool.h>
#include <stdint.h>

// ECU outputs read back by the bench. Both are low-side switches pulled up to
// 3V3, so a pulse is the LOW phase (injector open / probably ignition dwell).
typedef enum { CAP_IGN, CAP_INJ, CAP_COUNT } cap_channel_t;

// Readings older than this are reported as absent.
#define CAP_STALE_US 2000000u

typedef struct {
    bool valid;         // a pulse ended within CAP_STALE_US
    uint32_t count;     // completed pulses since boot
    uint32_t glitches;  // both edges seen in one interrupt, pulse shorter than the latency
    uint32_t period_us; // falling edge to falling edge, 0 if unknown
    uint32_t low_us;    // width of the last completed pulse
    bool have_angle;    // crank running and both edges seen after a reference
    float fall_deg;     // crank degrees after the last VW-18 falling edge
    float rise_deg;
} cap_result_t;

void capture_init(void);
cap_result_t capture_get(cap_channel_t ch, uint32_t ppr);
const char *capture_name(cap_channel_t ch);

// Hardware-independent edge bookkeeping, capture_core.c. The GPIO interrupt
// feeds it 64-bit microsecond timestamps and capture_get() reads it.
typedef struct {
    uint32_t count;
    uint32_t glitches;
    uint64_t fall_t;
    bool have_fall;
    uint32_t period_us;
    uint32_t low_us;
    uint64_t last_end_t;  // time of the rising edge that completed the last pulse
    // Delay from the last crank reference to the falling edge, and the
    // reference period at that moment.
    bool has_ref;
    uint32_t ref_dly_us;
    uint32_t ref_period_us;
} cap_chan_state_t;

typedef struct {
    bool have_ref;
    uint64_t ref_t;
    uint32_t ref_period_us;  // 0 until two references less than CAP_STALE_US apart
    cap_chan_state_t ch[CAP_COUNT];
} cap_state_t;

// Edge events latched for one pin during one interrupt.
#define CAP_EV_FALL 1u
#define CAP_EV_RISE 2u

void cap_on_ref(cap_state_t *s, uint64_t t);
void cap_on_edge(cap_state_t *s, cap_channel_t ch, bool rising, uint64_t t);
// Everything one GPIO interrupt saw, stamped with a single time taken on entry.
// ev[] are the latched CAP_EV_* bits per channel, high[] the pin levels read
// in the interrupt. Returns true if a crank reference was among them.
bool cap_on_irq(cap_state_t *s, uint64_t t, bool ref, const uint32_t ev[CAP_COUNT],
                const bool high[CAP_COUNT]);
cap_result_t cap_result(const cap_state_t *s, cap_channel_t ch, uint64_t now, uint32_t ppr);
