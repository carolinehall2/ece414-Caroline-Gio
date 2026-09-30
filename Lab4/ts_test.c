#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "ts_lcd.h"
#include <stdio.h>
#include "TFTMaster.h"


int main(){
    
    stdio_init_all();
    ts_lcd_init();
    tft_init_hw(); // Initializes the Pico's SPI and PIO pins
    tft_begin();   // Sends the startup commands to the LCD screen
    tft_setTextColor(0xFFFF); // Set text to white
    tft_setTextSize(2);       // Make text large enough to read easily

    uint16_t x=0, y=0;
    uint16_t last_x=0, last_y=0;
    bool has_touched = false;

    while (true) {
        if (get_ts_lcd(&x, &y)) {
            
            if (x != last_x || y != last_y || !has_touched) {
                last_x = x;
                last_y = y;
                has_touched = true;

                // Clear screen 
                tft_fillScreen(0x0000); 

                 // Draw horizontal and vertical lines for the crosshair
                tft_drawLine(x - 5, y, x + 5, y, 0xFFFF);
                tft_drawLine(x, y - 5, x, y + 5, 0xFFFF);

                // Format the numeric coordinates into a string
                char text[32];
                sprintf(text, "X: %u, Y: %u", x, y);

                // Move the invisible cursor to the top left
                tft_setCursor(10, 10);

                tft_writeString(text);
            }
        }
        
        // Small delay to debounce and prevent screen flickering
        sleep_ms(20); 
    }
    return 0;
}