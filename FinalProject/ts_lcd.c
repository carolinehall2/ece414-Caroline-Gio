#include "ts_lcd.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "knob_fsm.h"

#define X_PLUS 6
#define X_MINUS 7
#define Y_PLUS 8
#define Y_MINUS 9

#define TOUCH_X_CHANNEL 4
#define TOUCH_Y_CHANNEL 5

#define LCD_WIDTH 240
#define LCD_HEIGHT 320

#define ADC_MIN 400
#define ADC_MAX 3800

static const uint touch_pins[] = {
    X_PLUS, X_MINUS, Y_PLUS, Y_MINUS
};

static void release_touch_pins(void) {
    for (int i = 0; i < 4; ++i) {
        gpio_set_dir(touch_pins[i], GPIO_IN);
        gpio_disable_pulls(touch_pins[i]);
    }
}

static void select_mux(uint8_t channel) {
    gpio_put(MUX_S0, channel & 1u);
    gpio_put(MUX_S1, (channel >> 1) & 1u);
    gpio_put(MUX_S2, (channel >> 2) & 1u);
}

static uint16_t read_touch_adc(uint8_t channel) {
    select_mux(channel);

    adc_select_input(MUX_ADC);
    sleep_us(50);

    (void)adc_read();
    return adc_read();
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
        (int32_t)(size - 1) /
        (ADC_MAX - ADC_MIN);

    if (value < 0)
        value = 0;

    if (value >= size)
        value = size - 1;

    return (uint16_t)value;
}

void ts_lcd_init(void) {
    for (int i = 0; i < 4; ++i) {
        gpio_init(touch_pins[i]);
    }

    release_touch_pins();
    select_mux(0);
}

bool get_ts_lcd(uint16_t *px, uint16_t *py) {
    if (px == NULL || py == NULL)
        return false;

    release_touch_pins();

    gpio_put(X_PLUS, 1);
    gpio_put(X_MINUS, 1);

    gpio_set_dir(X_PLUS, GPIO_OUT);
    gpio_set_dir(X_MINUS, GPIO_OUT);

    gpio_pull_down(Y_PLUS);
    gpio_pull_down(Y_MINUS);

    sleep_us(30);

    bool touched =
        gpio_get(Y_PLUS) || gpio_get(Y_MINUS);

    release_touch_pins();

    if (!touched) {
        restore_audio_adc();
        return false;
    }

    gpio_put(X_PLUS, 1);
    gpio_put(X_MINUS, 0);

    gpio_set_dir(X_PLUS, GPIO_OUT);
    gpio_set_dir(X_MINUS, GPIO_OUT);

    uint16_t x_raw =
        read_touch_adc(TOUCH_X_CHANNEL);

    release_touch_pins();

    gpio_put(Y_PLUS, 1);
    gpio_put(Y_MINUS, 0);

    gpio_set_dir(Y_PLUS, GPIO_OUT);
    gpio_set_dir(Y_MINUS, GPIO_OUT);

    uint16_t y_raw =
        read_touch_adc(TOUCH_Y_CHANNEL);

    restore_audio_adc();

    uint16_t x =
        map_coordinate(x_raw, LCD_WIDTH);

    uint16_t y =
        map_coordinate(y_raw, LCD_HEIGHT);

    *px = LCD_WIDTH - 1 - x;
    *py = LCD_HEIGHT - 1 - y;

    return true;
}
