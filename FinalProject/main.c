#include <stdio.h>
#include "pico/stdlib.h"
#include "display_fsm.h"
#include "ts_lcd.h"
#include "knob_fsm.h"

int main(void) {
    stdio_init_all();
    sleep_ms(2000);

    knob_fsm_init();
    display_fsm_init();
    ts_lcd_init();

    uint32_t last_tick_display = 0;
    uint32_t last_tick_touch = 0;

    while (1) {
        uint32_t now = time_us_32();

        if (now - last_tick_display >= 33333) {
            last_tick_display = now;
            knob_fsm_tick();
            display_fsm_tick();
        }

        if (now - last_tick_touch >= 50000) {
            last_tick_touch = now;
            uint16_t px, py;

            if (get_ts_lcd(&px, &py)) {
                uint16_t screen_x = py;
                uint16_t screen_y = 240 - px;

                static uint32_t last_press = 0;
                if (now - last_press > 300000) {
                    if (screen_y >= 170 && screen_y <= 230) {
                        if (screen_x >= 15 && screen_x <= 80) {
                            toggle_effect(0);
                            last_press = now;
                        }
                        else if (screen_x >= 90 && screen_x <= 155) {
                            toggle_effect(1);
                            last_press = now;
                        }
                        else if (screen_x >= 165 && screen_x <= 230) {
                            toggle_effect(2);
                            last_press = now;
                        }
                        else if (screen_x >= 240 && screen_x <= 305) {
                            toggle_effect(3);
                            last_press = now;
                        }
                    }
                }
            }
        }
    }
}