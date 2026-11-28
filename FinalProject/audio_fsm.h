#ifndef AUDIO_FSM_H
#define AUDIO_FSM_H

#include "hardware/pio.h"

void audio_fsm_init(void);
void audio_fsm_tick(PIO pio, uint sm);

#endif
