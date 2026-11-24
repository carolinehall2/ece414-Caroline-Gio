
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "display_fsm.h"
#include "TFTMaster.h"
#include "ts_lcd.h"
#include "knob_fsm.h"

// Touchscreen signals use MUX channels 4 and 5.
// MUX output is Pico GP27 (ADC1).

int main(void) {
    stdio_init_all();

    sleep_ms(1500);

    printf("\n============================\n");
    printf("DJ BOARD TOUCHSCREEN TEST\n");
    printf("============================\n");

    // Initialize ADC for the MUX output
    adc_init();
    adc_gpio_init(MUX_SIG);

    // Initialize MUX selection pins
    gpio_init(MUX_S0);
    gpio_init(MUX_S1);
    gpio_init(MUX_S2);

    gpio_set_dir(MUX_S0, GPIO_OUT);
    gpio_set_dir(MUX_S1, GPIO_OUT);
    gpio_set_dir(MUX_S2, GPIO_OUT);

    gpio_put(MUX_S0, 0);
    gpio_put(MUX_S1, 0);
    gpio_put(MUX_S2, 0);

    // Initialize the touchscreen
    ts_lcd_init();

    // Initialize LCD and draw DJ interface
    display_fsm_init();

    printf("Initialization complete.\n");
    printf("Touch the screen now!\n\n");

    uint16_t touch_x = 0;
    uint16_t touch_y = 0;

    bool previous_pressed = false;

    while (true) {

        bool pressed = get_ts_lcd(
            &touch_x,
            &touch_y
        );

        if (pressed) {

            // Update on first touch
            if (!previous_pressed) {

                printf(
                    "TOUCH DETECTED: X=%u Y=%u\n",
                    touch_x,
                    touch_y
                );

                // Visible confirmation on the LCD
                tft_fillRect(
                    220, 5, 95, 24,
                    ILI9340_GREEN
                );

                tft_setCursor(230, 10);
                tft_setTextColor(ILI9340_BLACK);
                tft_setTextSize(2);
                tft_writeString("TOUCH!");
            }

        } else {

            if (previous_pressed) {

                printf("TOUCH RELEASED\n");

                // Clear touch indicator
                tft_fillRect(
                    220, 5, 95, 24,
                    ILI9340_BLACK
                );
            }
        }

        previous_pressed = pressed;

        sleep_ms(50);
    }

    return 0;
}
