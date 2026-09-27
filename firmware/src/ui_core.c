#include "ui_core.h"

#include <stdio.h>

#include "board.h"
#include "capture.h"
#include "crank.h"
#include "dac.h"
#include "gfx.h"
#include "knock.h"
#include "sense.h"

void ui_select_next(ui_state_t *ui) {
    ui->selected = (ui_item_t)((ui->selected + 1) % UI_ITEM_COUNT);
}

static uint32_t step(uint32_t v, int dir, uint32_t by, uint32_t max) {
    if (dir < 0) return v > by ? v - by : 0;
    return v + by < max ? v + by : max;
}

static void adjust_dac(dac_channel_t ch, int dir) {
    dac_set_mv(ch, step(dac_get_mv(ch), dir, UI_MV_STEP, dac_max_mv(ch)));
}

void ui_adjust(ui_state_t *ui, int dir) {
    switch (ui->selected) {
    case UI_RPM: crank_set_rpm(step(crank_get_rpm(), dir, UI_RPM_STEP, CRANK_RPM_MAX)); break;
    case UI_ECU: board_set_ecu_power(dir > 0); break;
    case UI_IDLE: board_set_idle_switch(dir > 0); break;
    case UI_KNOCK: knock_set_hz(step(knock_get_hz(), dir, UI_KNOCK_STEP, KNOCK_HZ_MAX)); break;
    case UI_AIR: adjust_dac(DAC_AIR, dir); break;
    case UI_WATER: adjust_dac(DAC_WATER, dir); break;
    case UI_AFM: adjust_dac(DAC_AFM, dir); break;
    case UI_LAMBDA: adjust_dac(DAC_LAMBDA, dir); break;
    default: break;
    }
}

bool ui_item_repeats(ui_item_t item) { return item != UI_ECU && item != UI_IDLE; }

// Each adjustable item is an 8-character field in the left or right column
// of the top four rows; the selected one is drawn inverted.
static void field(uint8_t *fb, const ui_state_t *ui, ui_item_t item, int col, int row,
                  const char *text) {
    char buf[9];
    snprintf(buf, sizeof buf, "%-8.8s", text);
    gfx_text(fb, col, row, buf, ui->selected == item);
}

void ui_render(const ui_state_t *ui, uint8_t *fb) {
    char s[GFX_COLS + 1];
    gfx_clear(fb);

    snprintf(s, sizeof s, "RPM %4lu", (unsigned long)crank_get_rpm());
    field(fb, ui, UI_RPM, 0, 0, s);
    field(fb, ui, UI_ECU, 12, 0, board_get_ecu_power() ? "ECU on" : "ECU off");

    field(fb, ui, UI_IDLE, 0, 1, board_get_idle_switch() ? "IDLE on" : "IDLE off");
    uint32_t hz = knock_get_hz();
    if (hz == 0) snprintf(s, sizeof s, "KN off");
    else snprintf(s, sizeof s, "KN%5lu%s", (unsigned long)hz, knock_get_burst().enabled ? "*" : "");
    field(fb, ui, UI_KNOCK, 12, 1, s);

    snprintf(s, sizeof s, "AIR %4lu", (unsigned long)dac_get_mv(DAC_AIR));
    field(fb, ui, UI_AIR, 0, 2, s);
    snprintf(s, sizeof s, "WAT %4lu", (unsigned long)dac_get_mv(DAC_WATER));
    field(fb, ui, UI_WATER, 12, 2, s);
    snprintf(s, sizeof s, "AFM %4lu", (unsigned long)dac_get_mv(DAC_AFM));
    field(fb, ui, UI_AFM, 0, 3, s);
    snprintf(s, sizeof s, "LAM %4lu", (unsigned long)dac_get_mv(DAC_LAMBDA));
    field(fb, ui, UI_LAMBDA, 12, 3, s);

    ina_reading_t ecu = sense_read_ecu();
    if (ecu.valid) snprintf(s, sizeof s, "ECU %5.3fA %5.2fV", ecu.current_a, ecu.bus_v);
    else snprintf(s, sizeof s, "ECU --");
    gfx_text(fb, 0, 4, s, false);

    ina_reading_t vlv = sense_read_valve();
    if (vlv.valid) snprintf(s, sizeof s, "VLV %5.3fA %5.2fV", vlv.current_a, vlv.bus_v);
    else snprintf(s, sizeof s, "VLV --");
    gfx_text(fb, 0, 5, s, false);

    cap_result_t ign = capture_get(CAP_IGN, crank_get_ppr());
    if (ign.have_angle) snprintf(s, sizeof s, "IGN %5.1f-%5.1f deg", ign.fall_deg, ign.rise_deg);
    else if (ign.valid) snprintf(s, sizeof s, "IGN low %lu us", (unsigned long)ign.low_us);
    else snprintf(s, sizeof s, "IGN --");
    gfx_text(fb, 0, 6, s, false);

    cap_result_t inj = capture_get(CAP_INJ, crank_get_ppr());
    if (inj.valid && inj.have_angle)
        snprintf(s, sizeof s, "INJ %5.2fms @%5.1f", inj.low_us / 1000.0f, inj.fall_deg);
    else if (inj.valid) snprintf(s, sizeof s, "INJ %5.2fms", inj.low_us / 1000.0f);
    else snprintf(s, sizeof s, "INJ --");
    gfx_text(fb, 0, 7, s, false);
}

bool btn_update(btn_t *b, bool pressed, uint32_t now_ms, bool repeat) {
    if (pressed != b->raw) {
        b->raw = pressed;
        b->changed_ms = now_ms;
    }
    if (b->raw != b->stable && now_ms - b->changed_ms >= BTN_DEBOUNCE_MS) {
        b->stable = b->raw;
        if (b->stable) {
            b->next_repeat_ms = now_ms + BTN_REPEAT_DELAY_MS;
            return true;
        }
        return false;
    }
    if (b->stable && repeat && (int32_t)(now_ms - b->next_repeat_ms) >= 0) {
        b->next_repeat_ms += BTN_REPEAT_MS;
        return true;
    }
    return false;
}
