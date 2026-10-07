#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "ts_lcd.h"
#include "TFTMaster.h"

typedef enum {
    INIT,
    WAITING_FOR_OP1,
    WAITING_FOR_OPERATOR,
    WAITING_FOR_OP2,
    DISPLAY_RESULT
} CalcState;

int32_t op1 = 0;
int32_t op2 = 0;
char current_operator = '\0';
CalcState state = INIT;

void draw_ui() {
    tft_fillScreen(ILI9340_BLACK); 
    
    uint16_t x_cols[4] = {6, 84, 163, 241};
    uint16_t y_rows[4] = {50, 99, 147, 196};
    
    char buttons[4][4] = {
        {'1', '2', '3', '+'},
        {'4', '5', '6', '-'},
        {'7', '8', '9', '*'},
        {'0', 'C', '=', '/'}
    };

    for(int r = 0; r < 4; r++) {
        for(int c = 0; c < 4; c++) {
            tft_drawRoundRect(x_cols[c], y_rows[r], 72, 43, 5, ILI9340_WHITE); 
            tft_drawChar(x_cols[c] + 30, y_rows[r] + 15, buttons[r][c], ILI9340_WHITE, ILI9340_BLACK, 2); 
        }
    }
}

// Reverted to your strict hitboxes to filter out noise jumps
char get_button_pressed(uint16_t x, uint16_t y) {
    if (y > 50 && y < 93) {
        if (x > 6 && x < 78) return '1';
        if (x > 84 && x < 156) return '2';
        if (x > 163 && x < 235) return '3';
        if (x > 241 && x < 313) return '+';
    } else if (y > 99 && y < 142) {
        if (x > 6 && x < 78) return '4';
        if (x > 84 && x < 156) return '5';
        if (x > 163 && x < 235) return '6';
        if (x > 241 && x < 313) return '-';
    } else if (y > 147 && y < 190) {
        if (x > 6 && x < 78) return '7';
        if (x > 84 && x < 156) return '8';
        if (x > 163 && x < 235) return '9';
        if (x > 241 && x < 313) return '*';
    } else if (y > 196 && y < 239) {
        if (x > 6 && x < 78) return '0';
        if (x > 84 && x < 156) return 'C';
        if (x > 163 && x < 235) return '=';
        if (x > 241 && x < 313) return '/';
    }
    return '\0';
}

void display_value(int32_t val) {
    char buf[16];
    sprintf(buf, "%ld", val);
    tft_fillRect(0, 0, 320, 50, ILI9340_BLACK); 
    tft_setCursor(10, 15); 
    tft_setTextColor(ILI9340_WHITE); 
    tft_setTextSize(3); 
    tft_writeString(buf); 
}

int main() {
    stdio_init_all();
    ts_lcd_init(); 
    tft_init_hw(); 
    tft_begin(); 
    tft_setRotation(3); 
    
    draw_ui();
    display_value(0);
    
    uint16_t raw_x, raw_y;
    bool touched_last_frame = false;

    while (1) {
        bool currently_touched = get_ts_lcd(&raw_x, &raw_y); 
        
        if (currently_touched && !touched_last_frame) {
            
            // Retained the coordinate translation so buttons match
            uint16_t mapped_x = 319 - raw_y;
            uint16_t mapped_y = raw_x; 

            char btn = get_button_pressed(mapped_x, mapped_y);
            
            if (btn == 'C') { 
                op1 = 0;
                op2 = 0;
                current_operator = '\0';
                state = INIT;
                display_value(0);
            } 
            else if (btn != '\0') {
                switch(state) {
                    case INIT:
                        if (btn >= '0' && btn <= '9') {
                            op1 = btn - '0';
                            state = WAITING_FOR_OP1;
                            display_value(op1);
                        }
                        break;
                        
                    case WAITING_FOR_OP1:
                        if (btn >= '0' && btn <= '9') {
                            op1 = (op1 * 10) + (btn - '0');
                            display_value(op1);
                        } else if (btn == '+' || btn == '-' || btn == '*' || btn == '/') {
                            current_operator = btn;
                            state = WAITING_FOR_OPERATOR;
                        }
                        break;
                        
                    case WAITING_FOR_OPERATOR:
                        if (btn >= '0' && btn <= '9') {
                            op2 = btn - '0';
                            state = WAITING_FOR_OP2;
                            display_value(op2);
                        }
                        break;
                        
                    case WAITING_FOR_OP2:
                        if (btn >= '0' && btn <= '9') {
                            op2 = (op2 * 10) + (btn - '0'); 
                            display_value(op2);
                        } else if (btn == '=') {
                            state = DISPLAY_RESULT;
                            int32_t result = 0;
                            
                            if (current_operator == '+') result = op1 + op2;
                            else if (current_operator == '-') result = op1 - op2;
                            else if (current_operator == '*') result = op1 * op2;
                            else if (current_operator == '/') {
                                if (op2 != 0) {
                                    result = op1 / op2;
                                }
                            }
                            
                            display_value(result);
                        }
                        break;
                        
                    case DISPLAY_RESULT:
                        break;
                }
            }
        }
        
        touched_last_frame = currently_touched;
        
        // Fast enough to not miss quick finger taps, slow enough to prevent SPI crashes
        sleep_ms(20); 
    }
}