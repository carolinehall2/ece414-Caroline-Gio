// Mono AUX input -> RP2040 ADC0 -> WM8960 stereo speakers.
// First pass-through prototype: ADC sampled in a PIO-paced loop (no DMA).
// PIO left-justified 16-bit stereo, 48 kHz, GP10=BCLK, GP11=LRCLK, GP12=DACDAT.
// I2C0 GP4=SDA, GP5=SCL. Waveshare HAT supplies a 24 MHz MCLK oscillator.
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "audio_i2s.pio.h"



#define WM8960_ADDR 0x1a
#define SAMPLE_RATE 48000
#define ADC_GPIO 26
#define ADC_CHANNEL 0
// Digital gain on the AC component of the 12-bit input.
// Start conservatively. Increase only after checking for distortion.
#define ADC_GAIN 80
// Smooth the slowly changing ADC bias: 2^12 samples ~85 ms at 48kHz.
#define DC_TRACK_SHIFT 12



// Two-byte WM8960 writes: 7-bit register number + 9-bit register value.
static bool wm_write(uint8_t reg, uint16_t val) {
   uint8_t bytes[2] = {
       (uint8_t)((reg << 1) | ((val >> 8) & 1)),
       (uint8_t)(val & 0xff)
   };
   int sent = i2c_write_timeout_us(i2c0, WM8960_ADDR, bytes, 2, false, 100000);
   if (sent != 2) {
       printf("FAIL: register %u (0x%02X), I2C result %d\n", reg, reg, sent);
       fflush(stdout);
       return false;
   }
   return true;
}



#define W(reg,val) do { if (!wm_write((reg),(val))) return false; } while (0)
static bool wm_init(void) {
   W(15, 0x000);     // software reset
   sleep_ms(20);
   W(25, 0x1C0);     // VMID fast start, enable VREF
   sleep_ms(100);
   W(25, 0x0C0);     // normal VMID, VREF still enabled



   // 24 MHz MCLK -> PLL prescale /2 -> 12 MHz.
   // PLL N=8, K=0x3126E8 yields SYSCLK = 12.288 MHz.
   W(52, 0x038);     // fractional mode, MCLK /2, N=8
   W(53, 0x031);     // K[23:16]
   W(54, 0x026);     // K[15:8]
   W(55, 0x0E8);     // K[7:0]
   W(26, 0x001);     // turn on PLL first
   sleep_ms(15);
   W(4,  0x005);     // Working speaker-test clock setting: PLL and SYSCLK divider
   W(7,  0x001);     // SLAVE, 16-bit LEFT-JUSTIFIED (matches PIO)
   W(8,  0x1C0);     // Working speaker-test Class-D clock setting
   W(5,  0x000);     // DAC unmuted; no de-emphasis
   W(10, 0x0FF);     // DAC L digital volume = 0 dB
   W(11, 0x1FF);     // DAC R digital volume = 0 dB, update
   W(34, 0x100);     // route left DAC to left output mixer
   W(37, 0x100);     // route right DAC to right output mixer
   W(47, 0x018);     // enable L and R output mixers
   W(40, 0x070);     // speaker left low volume (-25 dB approx)
   W(41, 0x170);     // speaker right low volume + update
   W(26, 0x199);     // DAC L/R, speaker L/R, PLL power on
   sleep_ms(100);
   W(49, 0x0F7);     // enable both Class-D outputs; keep reserved bits
   return true;
}
#undef W



static void setup_i2s(PIO pio, uint sm) {
   uint offset = pio_add_program(pio, &audio_i2s_tx_program);
   pio_sm_config c = audio_i2s_tx_program_get_default_config(offset);
   pio_gpio_init(pio, 10); // BCLK
   pio_gpio_init(pio, 11); // LRCLK
   pio_gpio_init(pio, 12); // DACDAT
   pio_sm_set_consecutive_pindirs(pio, sm, 10, 3, true);
   sm_config_set_sideset_pins(&c, 10);
   sm_config_set_out_pins(&c, 12, 1);
   sm_config_set_out_shift(&c, false, true, 32);  // MSB-first; auto-pull 32 bits
   sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
   // 66 instructions per stereo frame (see audio_i2s.pio)
   sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (SAMPLE_RATE * 66.0f));
   pio_sm_init(pio, sm, offset, &c);
}



// Downmix the single analog AUX channel to both output speakers.
// Remove the ~1.65V input bias with a slow-moving DC estimator.
static uint32_t next_stereo(int32_t *dc_q8) {
   uint16_t raw = adc_read();  // ADC0: 12-bit signal on GP26
   int32_t sample_q8 = (int32_t)raw << 8;
   *dc_q8 += (sample_q8 - *dc_q8) >> DC_TRACK_SHIFT;
   int32_t centered = (sample_q8 - *dc_q8) >> 8;
int32_t scaled = centered * ADC_GAIN;



   if (scaled > INT16_MAX) scaled = INT16_MAX;
   if (scaled < INT16_MIN) scaled = INT16_MIN;
   uint16_t mono = (uint16_t)(int16_t)scaled;
   return ((uint32_t)mono << 16) | mono;
}



int main(void) {
   stdio_init_all();
   sleep_ms(1500);
   printf("WM8960 ADC-to-speakers passthrough starting...\n");



   // Codec control (same I2C pins as the successful tests).
   i2c_init(i2c0, 100000);
   gpio_set_function(4, GPIO_FUNC_I2C);
   gpio_set_function(5, GPIO_FUNC_I2C);
   gpio_pull_up(4);
   gpio_pull_up(5);



   // Mono AUX circuit: 1uF coupling capacitor, 1k series,
   // two 10k bias resistors, with GP26 at the bias node.
   adc_init();
   adc_gpio_init(ADC_GPIO);
   adc_select_input(ADC_CHANNEL);
   int32_t dc_q8 = (int32_t)adc_read() << 8;



   // Pico onboard LED blinks while audio is streaming.
   gpio_init(PICO_DEFAULT_LED_PIN);
   gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);



   PIO pio = pio0;
   uint sm = pio_claim_unused_sm(pio, true);
   setup_i2s(pio, sm);
   // Fill TX FIFO with silence before starting the codec.
    printf("Starting I2S...\n");
    fflush(stdout);

    pio_sm_put_blocking(pio, sm, 0);
    pio_sm_put_blocking(pio, sm, 0);

    pio_sm_set_enabled(pio, sm, true);

    printf("I2S started succcessfully \n");
    fflush(stdout);



   if (!wm_init()) {
       printf("Codec initialization failed. Check I2C/power.\n");
       while (true) sleep_ms(2000);
   }
   printf("Streaming GP26 ADC to stereo speakers (mono passthrough).\n");
   fflush(stdout);



   // The PIO's 48k frame rate paces these ADC conversions.
   // Deliberately no printf in the audio loop: USB printing may cause clicks.
   uint32_t frames = 0;
   absolute_time_t next_report = make_timeout_time_ms(2000);
   printf("Entering audio loop...\n");
   fflush(stdout);
   while (true) {
       if(!pio_sm_is_tx_fifo_full(pio, sm)) {
           uint32_t stereo = next_stereo(&dc_q8);
           pio_sm_put(pio, sm, stereo);
           frames++;
       }
       if(time_reached(next_report)){
        printf("Audio loop running: %u frames sent\n", frames);
        fflush(stdout);
        next_report = make_timeout_time_ms(2000);
       }
   }
}



