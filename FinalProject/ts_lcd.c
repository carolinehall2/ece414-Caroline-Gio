#include "ts_lcd.h"

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "knob_fsm.h"

#define X_PLUS   6u
#define X_MINUS  7u
#define Y_PLUS   8u
#define Y_MINUS  9u

#define TOUCH_X_CHANNEL 4u
#define TOUCH_Y_CHANNEL 5u

#define TOUCH_WIDTH  240u
#define TOUCH_HEIGHT 320u

#define ADC_MIN 400
#define ADC_MAX 3800

#define TOUCH_HIGH_MIN 250u
#define TOUCH_LOW_MAX  3845u

#define TOUCH_DEBUG 1

static const uint touch_pins[4] = {
    X_PLUS, X_MINUS, Y_PLUS, Y_MINUS
};

static void release_touch_pins(void) {
    for (uint i = 0; i < 4; ++i) {
        gpio_set_dir(touch_pins[i], GPIO_IN);
        gpio_disable_pulls(touch_pins[i]);
    }
}

static void drive_pin(uint pin, bool level) {
    gpio_put(pin, level);
    gpio_set_dir(pin, GPIO_OUT);
}

static void select_mux(uint8_t channel) {
    gpio_put(MUX_S0, (channel >> 0) & 1u);
    gpio_put(MUX_S1, (channel >> 1) & 1u);
    gpio_put(MUX_S2, (channel >> 2) & 1u);
}

static uint16_t read_mux_adc(uint8_t channel) {
    select_mux(channel);
    adc_select_input(MUX_ADC);

    sleep_us(100);

    (void)adc_read();

    uint32_t sum = 0;

    for (int i = 0; i < 4; ++i) {
        sum += adc_read();
    }

    return (uint16_t)((sum + 2u) / 4u);
}

static void restore_audio_adc(void) {
    release_touch_pins();

    select_mux(0);

    adc_select_input(0);

    (void)adc_read();
}

static uint16_t map_coordinate(
    uint16_t raw,
    uint16_t size
) {
    int32_t value =
        ((int32_t)raw - ADC_MIN) *
        (int32_t)(size - 1u) /
        (ADC_MAX - ADC_MIN);

    if (value < 0) {
        value = 0;
    }

    if (value >= size) {
        value = (int32_t)size - 1;
    }

    return (uint16_t)value;
}

void ts_lcd_init(void) {
    for (uint i = 0; i < 4; ++i) {
        gpio_init(touch_pins[i]);
    }

    release_touch_pins();

    select_mux(0);
}

bool get_ts_lcd(uint16_t *px, uint16_t *py) {
    if (px == NULL || py == NULL) {
        return false;
    }

    release_touch_pins();

    drive_pin(X_PLUS, true);
    drive_pin(X_MINUS, true);

    gpio_pull_down(Y_PLUS);

    uint16_t high_read =
        read_mux_adc(TOUCH_X_CHANNEL);

    release_touch_pins();

    drive_pin(X_PLUS, false);
    drive_pin(X_MINUS, false);

    gpio_pull_up(Y_PLUS);

    uint16_t low_read =
        read_mux_adc(TOUCH_X_CHANNEL);

    bool pressed =
        (high_read > TOUCH_HIGH_MIN) &&
        (low_read < TOUCH_LOW_MAX);

    uint16_t x_raw = 0;
    uint16_t y_raw = 0;

    if (pressed) {
        release_touch_pins();

        drive_pin(X_PLUS, true);
        drive_pin(X_MINUS, false);

        x_raw =
            read_mux_adc(TOUCH_X_CHANNEL);

        release_touch_pins();

        drive_pin(Y_PLUS, true);
        drive_pin(Y_MINUS, false);

        y_raw =
            read_mux_adc(TOUCH_Y_CHANNEL);

        uint16_t x =
            map_coordinate(x_raw, TOUCH_WIDTH);

        uint16_t y =
            map_coordinate(y_raw, TOUCH_HEIGHT);

        *px = (uint16_t)(TOUCH_WIDTH - 1u - x);
        *py = (uint16_t)(TOUCH_HEIGHT - 1u - y);
    }

    restore_audio_adc();

#if TOUCH_DEBUG
    static uint32_t last_debug_us = 0;

    uint32_t now = time_us_32();

    if ((uint32_t)(now - last_debug_us) >= 250000u) {
        last_debug_us = now;

        if (pressed) {
            printf(
                "TOUCH: Xraw=%u Yraw=%u "
                "X=%u Y=%u "
                "testHigh=%u testLow=%u\n",
                x_raw,
                y_raw,
                *px,
                *py,
                high_read,
                low_read
            );
        } else {
            printf(
                "NO TOUCH: "
                "testHigh=%u testLow=%u\n",
                high_read,
                low_read
            );
        }

        fflush(stdout);
    }
#endif

    return pressed;
}
