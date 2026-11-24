
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "display_fsm.h"
#include "TFTMaster.h"
#include "ts_lcd.h"
#include "knob_fsm.h"

int main(void) {
    stdio_init_all();

    // Give USB serial time to connect.
    sleep_ms(3000);

    printf("\n============================\n");
    printf("DJ BOARD TOUCHSCREEN TEST\n");
    printf("============================\n");
    fflush(stdout);

    // Initialize ADC for the multiplexer output (GP27).
    adc_init();
    adc_gpio_init(MUX_SIG);

    // Initialize multiplexer selection pins.
    gpio_init(MUX_S0);
    gpio_init(MUX_S1);
    gpio_init(MUX_S2);

    gpio_set_dir(MUX_S0, GPIO_OUT);
    gpio_set_dir(MUX_S1, GPIO_OUT);
    gpio_set_dir(MUX_S2, GPIO_OUT);

    gpio_put(MUX_S0, 0);
    gpio_put(MUX_S1, 0);
    gpio_put(MUX_S2, 0);

    // Initialize touchscreen GPIOs.
    ts_lcd_init();

    // Initialize LCD and display the DJ interface.
    display_fsm_init();

    printf("Initialization complete.\n");
    printf("Touch the screen now!\n\n");
    fflush(stdout);

    uint16_t touch_x = 0;
    uint16_t touch_y = 0;

    bool previous_pressed = false;
    uint32_t last_heartbeat = 0;

    while (true) {

        uint32_t now =
            to_ms_since_boot(get_absolute_time());

        // Print once every second, even with no touch.
        if ((uint32_t)(now - last_heartbeat) >= 1000) {
            last_heartbeat = now;

            printf("Pico running - waiting for touch...\n");
            fflush(stdout);
        }

        // Read touchscreen.
        bool pressed = get_ts_lcd(
            &touch_x,
            &touch_y
        );

        if (pressed && !previous_pressed) {

            printf(
                "TOUCH DETECTED: X=%u Y=%u\n",
                touch_x,
                touch_y
            );
            fflush(stdout);

            // Green indicator on LCD.
            tft_fillRect(
                220, 5, 95, 24,
                ILI9340_GREEN
            );

            tft_setCursor(230, 10);
            tft_setTextColor(ILI9340_BLACK);
            tft_setTextSize(2);
            tft_writeString("TOUCH!");
        }

        if (!pressed && previous_pressed) {

            printf("TOUCH RELEASED\n");
            fflush(stdout);

            // Remove green indicator.
            tft_fillRect(
                220, 5, 95, 24,
                ILI9340_BLACK
            );
        }

        previous_pressed = pressed;

        sleep_ms(50);
    }

    return 0;
}
