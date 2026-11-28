#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "knob_fsm.h"
#include "system_state.h"

#define PHONE_ADC 0
#define KNOB_INTERVAL_US 2000u
#define MUX_SETTLE_US 50u

uint16_t volume_val = 4095;
uint16_t bass_val = 2048;
uint16_t mid_val = 2048;
uint16_t treble_val = 2048;

static enum {SELECT, SETTLE,READ,UPDATE} state;

static uint8_t channel;
static uint32_t deadline;
static uint16_t pending;
static uint16_t *const values[] = {&volume_val,&bass_val,&mid_val,&treble_val};
static void select_mux(uint8_t ch) {gpio_put(MUX_S0, ch & 1u); gpio_put(MUX_S1, (ch >> 1) & 1u);gpio_put(MUX_S2, (ch >> 2) & 1u);}

static uint16_t read_mux(void) {
    adc_select_input(MUX_ADC);
    (void)adc_read();
    uint16_t value = adc_read();
    adc_select_input(PHONE_ADC);
    (void)adc_read();
    return value;
}

static void publish(void) {
    g_state.volume =(uint8_t)((volume_val * 100u + 2047u) / 4095u);
    g_state.bass =(uint8_t)((bass_val * 100u + 2047u) / 4095u);
    g_state.mid =(uint8_t)((mid_val * 100u + 2047u) / 4095u);
    g_state.treble =(uint8_t)((treble_val * 100u + 2047u) / 4095u);
    system_state_publish_knobs();
}

void knob_fsm_restart_scan(void) {
    select_mux(0);
    channel = 0;
    state = SELECT;
    deadline = time_us_32();
}

void knob_fsm_init(void) {
    gpio_init(MUX_S0);
    gpio_init(MUX_S1);
    gpio_init(MUX_S2);
    gpio_set_dir(MUX_S0, GPIO_OUT);
    gpio_set_dir(MUX_S1, GPIO_OUT);
    gpio_set_dir(MUX_S2, GPIO_OUT);
    select_mux(0);
    adc_gpio_init(MUX_SIG);
    for (uint8_t ch = 0; ch < 4; ++ch) {
        select_mux(ch);
        sleep_us(MUX_SETTLE_US);
        *values[ch] = read_mux();
    }
    publish();
    knob_fsm_restart_scan();
}

void knob_fsm_tick(void) {
    uint32_t now = time_us_32();
    switch (state) {
        
        case SELECT:
            if ((int32_t)(now - deadline) < 0)
                return;
            select_mux(channel);
            deadline = now + MUX_SETTLE_US;
            state = SETTLE;
            break;
        
        case SETTLE:
            if ((int32_t)(now - deadline) >= 0)
                state = READ;
            break;
        
        case READ:
            pending = read_mux();
            state = UPDATE;
            break;
        
        case UPDATE:
            *values[channel] =(uint16_t)((3u * *values[channel] + pending + 2u) / 4u);
            publish();
            channel = (channel + 1u) & 3u;
            deadline = now + KNOB_INTERVAL_US;
            state = SELECT;
            break;
    }
}
