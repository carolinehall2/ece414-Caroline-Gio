#include "audio_fsm.h"
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "knob_fsm.h"
#include "system_state.h"

#define AUDIO_ADC_CHANNEL 0
#define AUDIO_GAIN 32

typedef enum {playing, muted} AudioState;

static AudioState audio_state = playing;
static int32_t dc_bias;
static int32_t bass_low;
static int32_t upper_low;
static int32_t mid_smooth;
static int32_t treble_smooth;

void audio_fsm_init(void){
  adc_select_input(AUDIO_ADC_CHANNEL);
  sleep_ms(100);
  uint32_t bias_sum = 0;
  for(int i =0; i<256; i++){
    bias_sum += adc_read();
    sleep_us(20);
  }
  dc_bias = (int32_t)(bias_sum / 256u) * 1024;
  bass_low = 0;
  upper_low = 0;
  mid_smooth = 2048 * 256;
  treble_smooth = 2048 * 256;
  audio_state = g_state.mute ? muted : playing;
}

void audio_fsm_tick(PIO pio, uint sm) {
    switch (audio_state) {
        case playing:
            if (g_state.mute) {
                audio_state = muted;
            }
            break;

        case muted:
            if (!g_state.mute) {
                audio_state = playing;
            }
            break;
    }
  uint16_t raw = adc_read();
  dc_bias += ((int32_t)raw*1024 - dc_bias) /1024;
  int32_t centered = (int32_t)raw - (dc_bias/1024);
  bass_low += ((centered * 256) - bass_low) / 22;
  int32_t low = bass_low / 256;
  upper_low += ((centered * 256) - upper_low) / 4;
  int32_t upper = upper_low / 256;
  int32_t mid_band = upper - low;
  int32_t treble_band = centered - upper;
  mid_smooth += (((int32_t)mid_val * 256) - mid_smooth) / 64;
  treble_smooth += (((int32_t)treble_val * 256) - treble_smooth) / 64;

  int32_t mid_adjust = (mid_smooth / 256) - 2048;
  int32_t treble_adjust = (treble_smooth / 256) - 2048;
  int32_t bass_adjust = (int32_t)bass_val - 2048;
  int32_t bass_processed;
  
  if (bass_adjust >= 0) {
      bass_processed = centered + (low * bass_adjust * 3) / 2048;
  } else {
      bass_processed = centered + (low * bass_adjust) / 2048;
  }

  if (g_state.bass_boost) {
        bass_processed += low;
  }
  
  int32_t eq_processed = bass_processed;
  eq_processed += (mid_band * mid_adjust) / 2048;
  eq_processed += (treble_band * treble_adjust) / 2048;
  int32_t amplified = eq_processed * AUDIO_GAIN;
  amplified = (int32_t)(((int64_t)amplified * volume_val) / 4095);

  if (g_state.bitcrusher) {
      amplified = (amplified / 2048) * 2048;
  }

  if (audio_state == muted) {
        amplified = 0;
    }

    if (amplified > 32767) {
        amplified = 32767;
    }
    if (amplified < -32768) {
        amplified = -32768;
    }

  int16_t sample = (int16_t)amplified;
  uint16_t audio16 = (uint16_t)sample;
  uint32_t stereo = ((uint32_t)audio16 << 16) | (uint32_t)audio16;
  pio_sm_put_blocking(pio, sm, stereo);
}

