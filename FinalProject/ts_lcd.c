
#include "ts_lcd.h"
#include <stddef.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "knob_fsm.h"

#define X_PLUS 6u
#define X_MINUS 7u
#define Y_PLUS 8u
#define Y_MINUS 9u

#define TOUCH_X_CHANNEL 4u
#define TOUCH_Y_CHANNEL 5u

#define LCD_WIDTH 240u
#define LCD_HEIGHT 320u
#define ADC_MIN 400
#define ADC_MAX 3800
#define CONTACT_SUM_MIN 1700u
#define CONTACT_SUM_MAX 6200u

static const uint touch_pins[] = {
    X_PLUS, X_MINUS, Y_PLUS, Y_MINUS
};

static void release_touch_pins(void) {
    for (uint i = 0; i < 4; ++i) {
        gpio_set_dir(touch_pins[i], GPIO_IN);
        gpio_disable_pulls(touch_pins[i]);
    }
}

static void select_mux(uint8_t channel) {
    gpio_put(MUX_S0, channel & 1u);
    gpio_put(MUX_S1, (channel >> 1) & 1u);
    gpio_put(MUX_S2, (channel >> 2) & 1u);
}

static uint16_t read_mux(uint8_t channel) {
    select_mux(channel);
    adc_select_input(MUX_ADC);
    sleep_us(60);

    (void)adc_read();

    uint32_t sum = 0;

    for (uint i = 0; i < 3; ++i) {
        sum += adc_read();
    }

    return (uint16_t)((sum + 1u) / 3u);
}

static uint16_t read_axis(
    uint drive_plus,
    uint drive_minus,
    uint sense_pin,
    uint8_t channel,
    bool reversed
) {
    release_touch_pins();

    gpio_pull_down(sense_pin);

    gpio_put(drive_plus, reversed ? 0 : 1);
    gpio_put(drive_minus, reversed ? 1 : 0);

    gpio_set_dir(drive_plus, GPIO_OUT);
    gpio_set_dir(drive_minus, GPIO_OUT);

    return read_mux(channel);
}

static uint16_t map_coordinate(
    uint16_t raw,
    uint16_t size
) {
    int32_t value =
        ((int32_t)raw - ADC_MIN) *
        ((int32_t)size - 1) /
        (ADC_MAX - ADC_MIN);

    if (value < 0) {
        value = 0;
    }

    if (value >= size) {
        value = size - 1;
    }

    return (uint16_t)value;
}

static void restore_audio_adc(void) {
    release_touch_pins();
    select_mux(0);
    adc_select_input(0);
    (void)adc_read();
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

    uint16_t x_forward = read_axis(
        X_PLUS, X_MINUS, Y_PLUS,
        TOUCH_X_CHANNEL, false
    );

    uint16_t x_reverse = read_axis(
        X_PLUS, X_MINUS, Y_PLUS,
        TOUCH_X_CHANNEL, true
    );

    uint16_t y_forward = read_axis(
        Y_PLUS, Y_MINUS, X_PLUS,
        TOUCH_Y_CHANNEL, false
    );

    uint16_t y_reverse = read_axis(
        Y_PLUS, Y_MINUS, X_PLUS,
        TOUCH_Y_CHANNEL, true
    );

    restore_audio_adc();

    uint32_t x_sum =
        (uint32_t)x_forward + x_reverse;

    uint32_t y_sum =
        (uint32_t)y_forward + y_reverse;

    if (x_sum < CONTACT_SUM_MIN ||
        x_sum > CONTACT_SUM_MAX ||
        y_sum < CONTACT_SUM_MIN ||
        y_sum > CONTACT_SUM_MAX) {
        return false;
    }

    uint16_t x_raw = (uint16_t)(
        ((uint32_t)x_forward +
         4095u - x_reverse + 1u) / 2u
    );

    uint16_t y_raw = (uint16_t)(
        ((uint32_t)y_forward +
         4095u - y_reverse + 1u) / 2u
    );

    uint16_t x =
        map_coordinate(x_raw, LCD_WIDTH);

    uint16_t y =
        map_coordinate(y_raw, LCD_HEIGHT);

    *px = (uint16_t)(LCD_WIDTH - 1u - x);
    *py = (uint16_t)(LCD_HEIGHT - 1u - y);

    return true;
}
