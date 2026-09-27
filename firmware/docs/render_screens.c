// Renders the OLED screens used in OPERATION.md: drives the real ui_core.c
// through a scripted session (same calls the buttons make) and writes each
// framebuffer as a PBM file. Sensor and capture readings are set here, since
// there is no hardware. Build and run via docs/make_images.sh.
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "capture.h"
#include "crank.h"
#include "dac.h"
#include "ecu.h"
#include "gfx.h"
#include "knock.h"
#include "sense.h"
#include "ui_core.h"

// --- readings shown on the screen, set per scene ---
static ina_reading_t ecu_reading, valve_reading;
static cap_result_t cap[CAP_COUNT];

ina_reading_t sense_read_ecu(void) { return ecu_reading; }
ina_reading_t sense_read_valve(void) { return valve_reading; }
void sense_init(void) {}

void capture_init(void) {}
cap_result_t capture_get(cap_channel_t ch, uint32_t ppr) {
    (void)ppr;
    return cap[ch];
}
const char *capture_name(cap_channel_t ch) { return ch == CAP_IGN ? "ign" : "inj"; }

static const char *out_dir;
static ui_state_t ui;

static void scene(const char *name) {
    uint8_t fb[GFX_FB_SIZE];
    ui_render(&ui, fb);
    char path[512];
    snprintf(path, sizeof path, "%s/%s.pbm", out_dir, name);
    FILE *f = fopen(path, "w");
    if (!f) {
        perror(path);
        return;
    }
    fprintf(f, "P1\n%d %d\n", GFX_WIDTH, GFX_HEIGHT);
    for (int y = 0; y < GFX_HEIGHT; y++) {
        for (int x = 0; x < GFX_WIDTH; x++) fputc(gfx_pixel(fb, x, y) ? '1' : '0', f);
        fputc('\n', f);
    }
    fclose(f);
    printf("wrote %s\n", path);
}

static void press(int dir, int times) {
    for (int i = 0; i < times; i++) ui_adjust(&ui, dir);
}

static void menu_to(ui_item_t item) {
    while (ui.selected != item) ui_select_next(&ui);
}

int main(int argc, char **argv) {
    out_dir = argc > 1 ? argv[1] : ".";

    // 12 V connected, no ECU yet: U1 sees the supply, the switched rail is off.
    ecu_reading = (ina_reading_t){true, 11.72f, 0.0f};
    valve_reading = (ina_reading_t){true, 0.0f, 0.0f};
    ui.selected = UI_RPM;
    scene("01_power_up");

    ui_select_next(&ui);
    scene("02_menu_ecu");

    press(+1, 1);
    ecu_reading = (ina_reading_t){true, 11.63f, 0.412f};
    valve_reading = (ina_reading_t){true, 11.61f, 0.0f};
    scene("03_ecu_on");

    // Warm idle: sensors set, crank at 850 rpm, ECU firing.
    dac_set_mv(DAC_AIR, 1800);
    dac_set_mv(DAC_WATER, 800);
    dac_set_mv(DAC_AFM, 1000);
    dac_set_mv(DAC_LAMBDA, 450);
    menu_to(UI_RPM);
    press(+1, 17);
    ecu_reading = (ina_reading_t){true, 11.58f, 0.468f};
    valve_reading = (ina_reading_t){true, 11.55f, 0.874f};
    cap[CAP_IGN] = (cap_result_t){.valid = true, .count = 512, .period_us = 35294,
                                  .low_us = 3920, .have_angle = true,
                                  .fall_deg = 138.0f, .rise_deg = 158.0f};
    cap[CAP_INJ] = (cap_result_t){.valid = true, .count = 256, .period_us = 70588,
                                  .low_us = 2450, .have_angle = true, .fall_deg = 62.5f};
    scene("04_idle_850");

    // Cold start: coolant NTC voltage raised.
    menu_to(UI_WATER);
    press(+1, 44);
    scene("05_water_cold");

    // Knock bursts (burst mode set over USB; the * marks it).
    knock_set_burst(10, 30, 1);
    menu_to(UI_KNOCK);
    press(+1, 14);
    scene("06_knock_burst");
    knock_set_hz(0);
    knock_burst_off();

    // Over-current trip.
    menu_to(UI_ECU);
    ecu_reading = (ina_reading_t){true, 11.40f, 3.1f};
    for (uint32_t t = 1; t <= ECU_TRIP_SAMPLES; t++) ecu_poll(t * ECU_POLL_MS);
    ecu_reading = (ina_reading_t){true, 11.73f, 0.0f};
    valve_reading = (ina_reading_t){true, 0.0f, 0.0f};
    cap[CAP_IGN] = (cap_result_t){0};
    cap[CAP_INJ] = (cap_result_t){0};
    scene("07_trip");

    // Crank stopped, ECU on: pulse widths without angles.
    press(-1, 1);
    press(+1, 1);
    menu_to(UI_RPM);
    press(-1, 17);
    ecu_reading = (ina_reading_t){true, 11.60f, 0.431f};
    valve_reading = (ina_reading_t){false, 0, 0};
    cap[CAP_IGN] = (cap_result_t){.valid = true, .count = 3, .low_us = 3920};
    cap[CAP_INJ] = (cap_result_t){.valid = true, .count = 1, .low_us = 6100};
    scene("08_crank_stopped");

    return 0;
}
