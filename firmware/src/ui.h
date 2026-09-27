#pragma once

// Standalone operation: OLED on J3 plus the MENU / - / + buttons. Call
// ui_poll() from the main loop; it never blocks except for OLED I2C writes.
void ui_init(void);
void ui_poll(void);
