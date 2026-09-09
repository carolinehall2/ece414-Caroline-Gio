#include "stdio.h"
#include "pico/stdlib.h"
#include "debounce_sw1.h"
#include "sw_in.h"

static bool BL;  //Left button pressed
static bool BR; //Right button pressed
static int count; //counter for LED
uint32_t timer;
uint32_t increment = 300; 

static enum Pong_States {INIT, SERVE_LEFT, SERVE_RIGHT, BALL_MOVING_LEFT, BALL_MOVING_RIGHT, FLASH_WINNER_RIGHT, FLASH_WINNER_LEFT} Pong_State;

void pong_init() {
    int min = 1;
    int max = 2;
    int random = (rand() % (max - min + 1)) + min;

    if(random == 1){
        Pong_State = SERVE_LEFT;
    }
    else{
        Pong_State = SERVE_RIGHT;
    }
}

void debounce_sw1_tick() {
    timer = timer_read();

    if(sw_in_read1() == 1){
        BL = true;
    }
    else{
        BL = false;
    }

    if(sw_in_read2() == 1){
        BR = true;
    }
    else{
        BR = false;
    }

    switch(Pong_State) {
        case INIT:
            pong_init();
            break;

        case SERVE_LEFT:
            count = 7; 
            if(BL == true){
                Pong_State = BALL_MOVING_RIGHT;
            }
            break;

        case SERVE_RIGHT:
            count = 0;
            if(BR == true){
                Pong_State = BALL_MOVING_LEFT;
            }
            break;

        case BALL_MOVING_LEFT:
            while(count < 8 && (timer < increment)){ //add a timer here 
                count++; 
                led_out_write (count);
                if(count == 7 && BR == true){
                    Pong_State = BALL_MOVING_RIGHT;
                }
                else if(count == 7 && BR == false){
                    Pong_State = FLASH_WINNER_RIGHT;
                }
                else if(count != 7 && BR == true){
                    Pong_State = FLASH_WINNER_RIGHT;
                }
                timer = 0;
            }
            break;

        case BALL_MOVING_RIGHT:
             while(count >= 0 && (timer < increment)){ //add a timer here 
                count++; 
                led_out_write (count);
                if(count == 0 && BL == true){
                    Pong_State = BALL_MOVING_LEFT;
                }
                else if(count == 0 && BL == false){
                    Pong_State = FLASH_WINNER_LEFT;
                }
                else if(count != 0 && BL == true){
                    Pong_State = FLASH_WINNER;
                }
                timer = 0; 
            }
            break;

        case FLASH_WINNER_RIGHT:
            int x = 0;
            while(x < 3){
                led_out_write (0x80); 
                delay_ms(1000);
                led_out_write (0x00);
                delay_ms(1000);
                x++;
            }

            break;

        case FLASH_WINNER_LEFT:
            int x = 0;
            while(x < 3){
                led_out_write (0x01); //flash the left LED three times (LED 7)
                delay_ms(1000);
                led_out_write (0x00);
                delay_ms(1000);
                x++;
            }
        break;
    }
    
}