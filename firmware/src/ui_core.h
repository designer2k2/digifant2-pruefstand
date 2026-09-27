#pragma once

#include <stdbool.h>
#include <stdint.h>

// Adjustable items, in the order MENU steps through them.
typedef enum {
    UI_RPM,
    UI_ECU,
    UI_IDLE,
    UI_KNOCK,
    UI_AIR,
    UI_WATER,
    UI_AFM,
    UI_LAMBDA,
    UI_ITEM_COUNT
} ui_item_t;

#define UI_RPM_STEP 50
#define UI_KNOCK_STEP 500
#define UI_MV_STEP 50

typedef struct {
    ui_item_t selected;
} ui_state_t;

void ui_select_next(ui_state_t *ui);
// dir = +1 or -1. On/off items: + switches on, - switches off (no toggle, so
// the ECU can't be powered by an accidental double press).
void ui_adjust(ui_state_t *ui, int dir);
bool ui_item_repeats(ui_item_t item);  // held +/- auto-repeats for numeric items
void ui_render(const ui_state_t *ui, uint8_t *fb);

// Button debounce: a level change must hold for BTN_DEBOUNCE_MS. A held
// button repeats after BTN_REPEAT_DELAY_MS, then every BTN_REPEAT_MS.
#define BTN_DEBOUNCE_MS 20
#define BTN_REPEAT_DELAY_MS 400
#define BTN_REPEAT_MS 100

typedef struct {
    bool raw;
    bool stable;
    uint32_t changed_ms;
    uint32_t next_repeat_ms;
} btn_t;

// Returns true when the button produces a press (initial or repeat).
bool btn_update(btn_t *b, bool pressed, uint32_t now_ms, bool repeat);
