#include "stdio.h"
#include "stdlib.h"
#include "pico/stdlib.h"
#include "timer.h"         
#include "sw_in.h"         
#include "led_out.h"       
#include "debounce_sw1.h"  

static bool BL;  
static bool BR; 
static int count;
static uint32_t t_last;
static uint32_t increment = 300; 

static enum Pong_States {
    INIT, 
    SERVE_LEFT, 
    SERVE_RIGHT, 
    BALL_MOVING_LEFT, 
    BALL_MOVING_RIGHT, 
    FLASH_WINNER_RIGHT, 
    FLASH_WINNER_LEFT
} Pong_State = INIT; // set start state to INIT when the program runs

// randomly decide who serves first
void pong_init() {
    int min = 0; // Minimum random value
    int max = 1; // Maximum random value
    // Generate a random number that is either 0 or 1
    int random = (rand() % (max - min + 1)) + min;

    // If the random number is 1, the left player serves
    if(random == 1){
        Pong_State = SERVE_LEFT;
    }
    // If the random number is 0, the right player serves
    else{
        Pong_State = SERVE_RIGHT;
    }
}


void game_tick() {
    // Check the time store it in t_now
    uint32_t t_now = timer_read();

    // Read the current hardware state of both buttons
    BL = sw_in_read1(); // Read Left button
    BR = sw_in_read2(); // Read Right button

    switch(Pong_State) {
        
        // Setup state
        case INIT:
            pong_init(); // Call the random serve function
            increment = 300; //need to make varible to chnage speed over time
            t_last = t_now; 
            break;

        // if left person serves
        case SERVE_LEFT:
            count = 7; // Set ball position to the far left LED (LED 7)
            led_out_write(count); // Turn on LED 7
            if(BL == true){ // If left player presses their button
                Pong_State = BALL_MOVING_RIGHT; // Change game state to moving right
                t_last = t_now; // Reset the timer
            }
            break;

        // if right person serves
        case SERVE_RIGHT:
            count = 0; // Set ball position to the far right LED (LED 0)
            led_out_write(1 << count); // Turn on LED 0
            if(BR == true){ // If right player presses their button
                Pong_State = BALL_MOVING_LEFT; // Change game state to moving left
                t_last = t_now; // Reset the timer
            }
            break;

        // Ball is traveling from right to left
        case BALL_MOVING_LEFT:
            if (timer_elapsed_ms(t_last, t_now) >= increment) {
                t_last = t_now; // Reset the timer for the next state
                count++; // Move the ball one LED to the left
                led_out_write(1 << count); // Update the LEDs to show the new position
                
                if(count == 7 && BR == true){ // If ball reaches the end and button is pressed
                    Pong_State = BALL_MOVING_RIGHT; // ball hit, reverse direction
                    if (increment > 100) increment -= 20; // Make faster by decreasing the delay
                }
                else if(count == 7 && BR == false){ // If ball reaches the end and button is not pressed
                   
                    Pong_State = FLASH_WINNER_LEFT; // Change state to flash the winner's LED
                }
                else if(count != 7 && BR == true){ // If button is pressed early (before the ball arrives)
                   
                    Pong_State = FLASH_WINNER_LEFT; // Change state to flash the winner's LED
                }
            }
            break;

        // Ball is currently traveling from left to right
        case BALL_MOVING_RIGHT:
            if (timer_elapsed_ms(t_last, t_now) >= increment) {
                t_last = t_now; // Reset the timer for the next state
                count--; // Move the ball one LED to the right
                led_out_write(1 << count); // Update the LEDs to show the new position
                
                if(count == 0 && BL == true){ // If ball reaches the end and button is pressed
                    Pong_State = BALL_MOVING_LEFT; // ball hit, reverse direction
                    if (increment > 100) increment -= 20; // Make faster by decreasing the delay
                }
                else if(count == 0 && BL == false){ // If ball reaches the end and button is not pressed
                
                    Pong_State = FLASH_WINNER_RIGHT; // Change state to flash the winner's LED
                }
                else if(count != 0 && BL == true){ // If button is pressed early 
                    Pong_State = FLASH_WINNER_RIGHT; // Change state to flash the winner's LED
                }
            }
            break;

        // The right player won the point
        case FLASH_WINNER_RIGHT: {
            int x = 0; //reset loop counter
            while(x < 4){ // Loop 4 times
                led_out_write(0x80); // Turn on the far left LED 
                sleep_ms(250); // keep the LED on for 250ms
                led_out_write(0x00); // Turn off all LEDs 
                sleep_ms(250); // Pause for 250ms
                x++; // add 1 to loop counter
            }
            Pong_State = INIT; // Send the state machine back to the beginning to start a new round
            break;
        }

        // The left player won the point
        case FLASH_WINNER_LEFT: {
            int x = 0; //reset loop counter
            while(x < 4){ // Loop  4 times
                led_out_write(0x01); // Turn on the far right LED
                sleep_ms(250); // keep the LED on for 250ms
                led_out_write(0x00); // Turn off all LEDs
                sleep_ms(250); // Pause for 250ms
                x++; // add 1 to loop counter
            }
            Pong_State = INIT; // Send the state machine back to the beginning to start a new round
            break;
        }
    }
}
// The main program start point
int main() {
    stdio_init_all();
    sw_in_init(); 
    led_out_init(); 

    while (1) {
        game_tick(); 
    }
}