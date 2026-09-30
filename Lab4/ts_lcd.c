#include "ts_lcd.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

// Hardware pin mappings from Lab 4 Manual 
#define Y_MINUS 21 
#define X_PLUS  22 
#define X_MINUS 26 
#define Y_PLUS  27 
#define Z_THRESHOLD 400

// Pico ADC channel mappings
#define ADC_CH_X_MINUS 0 // GPIO 26 is ADC0
#define ADC_CH_Y_PLUS  1 // GPIO 27 is ADC1

// Touchscreen parameters (
#define ADC_MIN     400  // Typical raw ADC minimum
#define ADC_MAX     3800 // Typical raw ADC maximum
#define LCD_WIDTH   240
#define LCD_HEIGHT  320

void ts_lcd_init(void) {
    adc_init();

}

bool get_ts_lcd(uint16_t *px, uint16_t *py) {
    uint16_t z1, z2, x_raw, y_raw;

    //  Read Pressure to   determine if the screen is currently touched
    gpio_init(X_PLUS);
    gpio_set_dir(X_PLUS, GPIO_OUT);
    gpio_put(X_PLUS, 0);

    gpio_init(Y_MINUS);
    gpio_set_dir(Y_MINUS, GPIO_OUT);
    gpio_put(Y_MINUS, 1);

    adc_gpio_init(X_MINUS);
    adc_gpio_init(Y_PLUS);

    adc_select_input(ADC_CH_X_MINUS);
    z1 = adc_read();
    adc_select_input(ADC_CH_Y_PLUS);
    z2 = adc_read();

    int pressure = z1 + (4095 - z2);

    // If pressure is below threshold, screen is untouched
    if (pressure < Z_THRESHOLD) {
        return false;
    }

    // Read X-axis coordinate
    gpio_init(X_MINUS);
    gpio_set_dir(X_MINUS, GPIO_OUT);
    gpio_put(X_MINUS, 0);

    gpio_init(X_PLUS);
    gpio_set_dir(X_PLUS, GPIO_OUT);
    gpio_put(X_PLUS, 1);

    // Let Y- float
    gpio_set_dir(Y_MINUS, GPIO_IN);

    adc_gpio_init(Y_PLUS);
    adc_select_input(ADC_CH_Y_PLUS);
    sleep_us(10); 
    x_raw = adc_read();

    //Read Y-axis coordinate
    gpio_init(Y_MINUS);
    gpio_set_dir(Y_MINUS, GPIO_OUT);
    gpio_put(Y_MINUS, 0);

    gpio_init(Y_PLUS);
    gpio_set_dir(Y_PLUS, GPIO_OUT);
    gpio_put(Y_PLUS, 1);

    // Let X+ float
    gpio_set_dir(X_PLUS, GPIO_IN);

    adc_gpio_init(X_MINUS);
    adc_select_input(ADC_CH_X_MINUS);
    sleep_us(10); 
    y_raw = adc_read();

    int mapped_x = ((x_raw - ADC_MIN) * LCD_WIDTH) / (ADC_MAX - ADC_MIN);
    int mapped_y = ((y_raw - ADC_MIN) * LCD_HEIGHT) / (ADC_MAX - ADC_MIN);

    mapped_x = LCD_WIDTH - mapped_x;
    mapped_y = LCD_HEIGHT - mapped_y;

    if (mapped_x < 0) mapped_x = 0;
    if (mapped_x >= LCD_WIDTH) mapped_x = LCD_WIDTH - 1;
    if (mapped_y < 0) mapped_y = 0;
    if (mapped_y >= LCD_HEIGHT) mapped_y = LCD_HEIGHT - 1;

    *px = (uint16_t)mapped_x;
    *py = (uint16_t)mapped_y;

    return true;
}