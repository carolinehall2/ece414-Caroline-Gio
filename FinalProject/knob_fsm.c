#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "knob_fsm.h"

// Define MUX Pins
#define S0 14
#define S1 15
#define S2 16
#define ADC_PIN 26

// global variables to hold the knob values
uint16_t volume_val = 0;
uint16_t bass_val   = 0;
uint16_t mid_val    = 0;
uint16_t treble_val = 0;

// SynchSM States
enum Knob_States { READ_VOL, READ_BASS, READ_MID, READ_TREBLE } knob_state;

void set_mux(int ch) {
    gpio_put(S0, ch & 1);
    gpio_put(S1, (ch >> 1) & 1);
    gpio_put(S2, (ch >> 2) & 1);
    sleep_us(10); // Small pause for the multiplexer to switch
}

void knob_fsm_init(void) {
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(0); // ADC0

    gpio_init(S0);
    gpio_init(S1);
    gpio_init(S2);
    gpio_set_dir(S0, GPIO_OUT);
    gpio_set_dir(S1, GPIO_OUT);
    gpio_set_dir(S2, GPIO_OUT);

    knob_state = READ_VOL;
}

void knob_fsm_tick(void) {
    // State Transitions & Actions
    switch (knob_state) {
        case READ_VOL:
            set_mux(0);
            volume_val = adc_read(); // 0 to 4095
            knob_state = READ_BASS;
            break;

        case READ_BASS:
            set_mux(1);
            bass_val = adc_read();
            knob_state = READ_MID;
            break;

        case READ_MID:
            set_mux(2);
            mid_val = adc_read();
            knob_state = READ_TREBLE;
            break;

        case READ_TREBLE:
            set_mux(3);
            treble_val = adc_read();
            knob_state = READ_VOL;
            break;

        default:
            knob_state = READ_VOL;
            break;
    }
}