#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

int main() {
    stdio_init_all();

    // Initialize ADC
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0); // ADC0 (GPIO 26)

    uint32_t last_tick_10ms = 0;

    while (1) {
        uint32_t now = time_us_32();

        // 10 ms task: sample potentiometer
        if (now - last_tick_10ms >= 10000) {
            last_tick_10ms = now;

            uint16_t raw = adc_read(); // 0 to 4095
            float percentage = (raw / 4095.0f) * 100.0f;

            printf("Potentiometer Raw: %4d | Level: %5.1f%%\n", raw, percentage);
        }
    }
}