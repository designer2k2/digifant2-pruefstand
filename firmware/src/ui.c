#include "ui.h"

#include "gfx.h"
#include "oled.h"
#include "pico/stdlib.h"
#include "pins.h"
#include "ui_core.h"

#define REFRESH_MS 200
#define OLED_RETRY_MS 2000

static ui_state_t ui;
static btn_t btn_menu, btn_minus, btn_plus;
static uint8_t fb[GFX_FB_SIZE];
static bool oled_ok;
static uint32_t next_refresh_ms, next_retry_ms;

void ui_init(void) { oled_ok = oled_init(); }

// Buttons pull to GND (internal pull-ups, see board.c).
static bool pressed(uint pin) { return !gpio_get(pin); }

void ui_poll(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    bool changed = false;

    if (btn_update(&btn_menu, pressed(PIN_BTN_MENU), now, false)) {
        ui_select_next(&ui);
        changed = true;
    }
    bool rep = ui_item_repeats(ui.selected);
    if (btn_update(&btn_plus, pressed(PIN_BTN_PLUS), now, rep)) {
        ui_adjust(&ui, +1);
        changed = true;
    }
    if (btn_update(&btn_minus, pressed(PIN_BTN_MINUS), now, rep)) {
        ui_adjust(&ui, -1);
        changed = true;
    }

    // A missing display is retried every couple of seconds so it can be
    // plugged in while the bench runs.
    if (!oled_ok) {
        if ((int32_t)(now - next_retry_ms) < 0) return;
        next_retry_ms = now + OLED_RETRY_MS;
        oled_ok = oled_init();
        if (!oled_ok) return;
        changed = true;
    }
    if (!changed && (int32_t)(now - next_refresh_ms) < 0) return;
    next_refresh_ms = now + REFRESH_MS;
    ui_render(&ui, fb);
    oled_ok = oled_flush(fb);
}
