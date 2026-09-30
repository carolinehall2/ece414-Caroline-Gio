#ifndef TS_LCD_H
#define TS_LCD_H

#include <stdbool.h>
#include <stdint.h>

// Initializes the ADC hardware for the touchscreen
void ts_lcd_init(void);

// Reads the touch status and maps raw analog data to screen coordinates
bool get_ts_lcd(uint16_t *px, uint16_t *py);

#endif