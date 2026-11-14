#ifndef KNOB_FSM_H
#define KNOB_FSM_H

#include <stdint.h>

#define MUX_S0 14 
#define MUX_s1 15
#define MUX_SIG 27
#define MUX_ADC 1

// global shared values
extern uint16_t volume_val;
extern uint16_t bass_val;
extern uint16_t mid_val;
extern uint16_t treble_val;

void knob_fsm_init(void);
void knob_fsm_tick(void);

#endif