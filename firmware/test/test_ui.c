// Checks button debounce/repeat, the menu actions, and renders the OLED
// framebuffer: printed as ASCII art and written to the PBM file given as
// argv[1] (if any) so the real 128x64 layout can be viewed as an image.
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "crank.h"
#include "dac.h"
#include "gfx.h"
#include "knock.h"
#include "ui_core.h"

static int failures;

static void check(const char *what, int ok) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static void press(ui_state_t *ui, int dir, int times) {
    for (int i = 0; i < times; i++) ui_adjust(ui, dir);
}

static void write_pbm(const uint8_t *fb, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "P1\n%d %d\n", GFX_WIDTH, GFX_HEIGHT);
    for (int y = 0; y < GFX_HEIGHT; y++) {
        for (int x = 0; x < GFX_WIDTH; x++) fputc(gfx_pixel(fb, x, y) ? '1' : '0', f);
        fputc('\n', f);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    // --- debounce and repeat ---
    btn_t b = {0};
    int events = 0;
    events += btn_update(&b, true, 0, true);
    events += btn_update(&b, false, 5, true);   // bounce
    events += btn_update(&b, true, 8, true);
    events += btn_update(&b, true, 20, true);   // only 12 ms stable
    check("bouncing press within 20 ms: no event yet", events == 0);
    check("press stable for 20 ms: one event", btn_update(&b, true, 28, true));
    events = 0;
    for (uint32_t t = 29; t < 427; t++) events += btn_update(&b, true, t, true);
    check("held: no repeat before 400 ms", events == 0);
    events = 0;
    for (uint32_t t = 427; t <= 728; t++) events += btn_update(&b, true, t, true);
    check("held: repeats at 428, 528, 628, 728 ms", events == 4);
    events = 0;
    for (uint32_t t = 729; t < 760; t++) events += btn_update(&b, false, t, true);
    check("release: no event", events == 0);
    btn_t nr = {0};
    events = 0;
    for (uint32_t t = 0; t < 2000; t++) events += btn_update(&nr, true, t, false);
    check("non-repeating item held 2 s: exactly one event", events == 1);

    // --- menu ---
    ui_state_t ui = {UI_RPM};
    for (int i = 0; i < UI_ITEM_COUNT; i++) ui_select_next(&ui);
    check("MENU cycles through all 8 items and wraps", ui.selected == UI_RPM);

    press(&ui, +1, 17);
    check("RPM +17 presses = 850", crank_get_rpm() == 850);
    press(&ui, -1, 100);
    check("RPM clamps at 0", crank_get_rpm() == 0);
    press(&ui, +1, 200);
    check("RPM clamps at 8000", crank_get_rpm() == CRANK_RPM_MAX);
    crank_set_rpm(850);

    ui.selected = UI_ECU;
    press(&ui, +1, 2);
    check("ECU: + switches on, a second + keeps it on", board_get_ecu_power());
    press(&ui, -1, 1);
    check("ECU: - switches off", !board_get_ecu_power());
    check("ECU and IDLE don't auto-repeat", !ui_item_repeats(UI_ECU) && !ui_item_repeats(UI_IDLE));

    ui.selected = UI_LAMBDA;
    press(&ui, +1, 41);
    check("lambda clamps at its 2047 mV maximum", dac_get_mv(DAC_LAMBDA) == 2047);
    dac_set_mv(DAC_LAMBDA, 450);

    ui.selected = UI_KNOCK;
    press(&ui, +1, 14);
    check("knock +14 presses = 7000 Hz", knock_get_hz() == 7000);
    press(&ui, -1, 20);
    check("knock - down to 0 = off", knock_get_hz() == 0);
    press(&ui, +1, 14);

    // --- render ---
    board_set_ecu_power(true);
    dac_set_mv(DAC_AIR, 1234);
    dac_set_mv(DAC_WATER, 2500);
    dac_set_mv(DAC_AFM, 1500);
    ui.selected = UI_RPM;
    uint8_t fb[GFX_FB_SIZE];
    ui_render(&ui, fb);

    int inverted_gap = 1, plain_gap = 1;
    for (int y = 0; y < 8; y++) {
        inverted_gap &= gfx_pixel(fb, 5, y);         // after 'R' of the selected RPM field
        plain_gap &= !gfx_pixel(fb, 12 * 6 + 5, y);  // after 'E' of the unselected ECU field
    }
    check("selected field drawn inverted, others normal", inverted_gap && plain_gap);

    for (int y = 0; y < GFX_HEIGHT; y++) {
        for (int x = 0; x < GFX_WIDTH; x++) putchar(gfx_pixel(fb, x, y) ? '#' : '.');
        putchar('\n');
    }
    if (argc > 1) write_pbm(fb, argv[1]);

    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
