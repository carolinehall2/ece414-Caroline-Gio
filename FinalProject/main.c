#include <stdio.h>
#include "pico/stdlib.h"
#include "knob_fsm.h"

int main(void) {
    stdio_init_all();
    knob_fsm_init();

    uint32_t last_tick = 0;
    uint32_t last_print = 0;

    while (1) {
        uint32_t now = time_us_32();

        // Tick Knob FSM every 10 ms 
        if (now - last_tick >= 10000) {
            last_tick = now;
            knob_fsm_tick();
        }

        // Print values every 100 ms
        if (now - last_print >= 100000) {
            last_print = now;
            printf("VOL: %4d | BASS: %4d | MID: %4d | TREBLE: %4d\n",
                   volume_val, bass_val, mid_val, treble_val);
        }
    }
}