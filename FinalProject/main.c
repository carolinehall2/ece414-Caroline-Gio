
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pio_instructions.h"
#include "hardware/adc.h"

#include "audio_i2s.pio.h"

#include "knob_fsm.h"
#include "system_state.h"

//Define Pins

#define WM8960_ADDR 0x1A
#define SAMPLE_RATE 48000

#define SDA_PIN 4
#define SCL_PIN 5

#define BCLK_PIN 10
#define LRCLK_PIN 11
#define DATA_PIN 12

#define AUDIO_INPUT_PIN 26
#define AUDIO_ADC_CHANNEL 0

#define AUDIO_GAIN 32

//Write to registers

static bool wm_write(uint8_t reg, uint16_t val) {

    uint8_t data[2] = {
        (uint8_t)((reg << 1) | ((val >> 8) & 1)),
        (uint8_t)(val & 0xFF)
    };

    int result = i2c_write_timeout_us(
        i2c0,
        WM8960_ADDR,
        data,
        2,
        false,
        100000
    );

    if (result != 2) {
        printf("FAIL: register %u, result %d\n",
               reg, result);

        fflush(stdout);
        return false;
    }

    return true;
}

#define W(reg, val) \
    do { if (!wm_write(reg, val)) return false; } while (0)

//initialize

static bool wm_init(void) {

    //Reset
    W(15, 0x000);
    sleep_ms(20);

    W(25, 0x1C0);
    sleep_ms(100);
    W(25, 0x0C0);

    W(52, 0x038);
    W(53, 0x031);
    W(54, 0x026);
    W(55, 0x0E8);

    W(26, 0x001);
    sleep_ms(15);

    //clocks and I2S
    W(4, 0x005);
    W(7, 0x00A);
    W(8, 0x1C0);


    W(5, 0x000);
    W(10, 0x0FF);
    W(11, 0x1FF);

    W(34, 0x100);
    W(37, 0x100);

    W(47, 0x018);

    //volume
    W(40, 0x070);
    W(41, 0x170);
    W(2, 0x079);
    W(3, 0x179);

    W(26, 0x1F9);
    sleep_ms(100);

    W(49, 0x0F7);

    return true;
}

#undef W

//audio output
static void setup_i2s(PIO pio, uint sm) {

    uint offset = pio_add_program(
        pio, &audio_i2s_program
    );

    pio_sm_config c =
        audio_i2s_program_get_default_config(offset);

    //i2s pins
    pio_gpio_init(pio, BCLK_PIN);
    pio_gpio_init(pio, LRCLK_PIN);
    pio_gpio_init(pio, DATA_PIN);

    sm_config_set_sideset_pins(&c, BCLK_PIN);
    sm_config_set_out_pins(&c, DATA_PIN, 1);

    sm_config_set_out_shift(&c, false, true, 32);
    
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    //clock divider
    float divider =
        (float)clock_get_hz(clk_sys) /
        (SAMPLE_RATE * 128.0f);

    sm_config_set_clkdiv(&c, divider);

    pio_sm_init(pio, sm, offset, &c);

    pio_sm_set_consecutive_pindirs(
        pio, sm, BCLK_PIN, 3, true
    );

    pio_sm_exec(
        pio,
        sm,
        pio_encode_jmp(
            offset + audio_i2s_offset_entry_point
        )
    );
}

//phone input
static void setup_audio_input(void) {

    adc_init();

    adc_gpio_init(AUDIO_INPUT_PIN);
    adc_select_input(AUDIO_ADC_CHANNEL);

    printf("Audio input configured on GP26\n");
    fflush(stdout);
}


int main(void) {

    stdio_init_all();
    sleep_ms(2000);

    printf("STARTING PHONE AUDIO PLAYBACK\n");
    fflush(stdout);

    i2c_init(i2c0, 100000);

    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    printf("I2C configured on GP%d/GP%d\n",
           SDA_PIN, SCL_PIN);

    fflush(stdout);


    printf("Initializing WM8960...\n");
    fflush(stdout);

    bool codec_ready = wm_init();

    if (!codec_ready) {

        while (true) {

            printf("ERROR: WM8960 initialization failed\n");
            fflush(stdout);

            sleep_ms(2000);
        }
    }

    printf("WM8960 register writes successful\n");
    fflush(stdout);

    setup_audio_input();
    // Initialize shared system state
    system_state_init();

    // Initialize potentiometers
    knob_fsm_init();

    sleep_ms(100);

    uint32_t bias_sum = 0;

    for (int i = 0; i < 256; i++) {
        bias_sum += adc_read();
        sleep_us(20);
    }

    int32_t initial_bias = bias_sum / 256;

    printf("ADC bias: %ld\n", (long)initial_bias);
    fflush(stdout);

    int32_t dc_bias = initial_bias * 1024;

    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);

    setup_i2s(pio, sm);

    pio_sm_put_blocking(pio, sm, 0);
    pio_sm_put_blocking(pio, sm, 0);
    pio_sm_set_enabled(pio, sm, true);

    printf("I2S enabled: GP10/GP11/GP12\n");
    printf("PHONE AUDIO RUNNING\n");
    fflush(stdout);
    int32_t bass_low = 0; 

    //audio loop
    while (true) {
        uint16_t raw = adc_read();

        dc_bias +=
            ((int32_t)raw * 1024 - dc_bias) / 1024;

        // Remove DC bias
int32_t centered =
    (int32_t)raw - (dc_bias / 1024);

    // Extract low frequencies using a low-pass filter
    // ======================================
// BASS CONTROL
// ======================================

// Extract bass frequencies
bass_low +=
    ((centered * 256) - bass_low) / 22;

int32_t low = bass_low / 256;

// Bass knob: -2048 to +2047
int32_t bass_adjust =
    (int32_t)bass_val - 2048;

// Increase bass boost strength
int32_t bass_processed;

if (bass_adjust >= 0) {

    // Stronger bass boost
    bass_processed =
        centered + (low * bass_adjust * 3) / 2048;

} else {

    // Reduce bass frequencies
    bass_processed =
        centered + (low * bass_adjust) / 2048;
}

        if (amplified > 32767) {
            amplified = 32767;
        }

        if (amplified < -32768) {
            amplified = -32768;
        }

        int16_t sample = (int16_t)amplified;

        uint32_t audio_word =
            ((uint32_t)(uint16_t)sample) << 16;

        pio_sm_put_blocking(pio, sm, audio_word);
        pio_sm_put_blocking(pio, sm, audio_word);

        // Run one step of the potentiometer FSM
        knob_fsm_tick();
    }
}
