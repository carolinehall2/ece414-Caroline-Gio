
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

// ==========================================
// PIN DEFINITIONS
// ==========================================

#define WM8960_ADDR 0x1A
#define SAMPLE_RATE 48000

#define SDA_PIN 4
#define SCL_PIN 5

#define BCLK_PIN 10
#define LRCLK_PIN 11
#define DATA_PIN 12

#define AUDIO_INPUT_PIN 26
#define AUDIO_ADC_CHANNEL 0

#define AUDIO_GAIN 16

// ==========================================
// WM8960 REGISTER WRITE
// ==========================================

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

// ==========================================
// WM8960 INITIALIZATION
// ==========================================

static bool wm_init(void) {

    // Reset codec
    W(15, 0x000);
    sleep_ms(20);

    // Enable reference voltage and VMID
    W(25, 0x1C0);
    sleep_ms(100);
    W(25, 0x0C0);

    // Configure PLL
    W(52, 0x038);
    W(53, 0x031);
    W(54, 0x026);
    W(55, 0x0E8);

    // Enable PLL
    W(26, 0x001);
    sleep_ms(15);

    // Configure clocks and I2S format
    W(4, 0x005);
    W(7, 0x00A);  // 24-bit I2S
    W(8, 0x1C0);

    // Enable DAC audio
    W(5, 0x000);

    // Left and right DAC volume
    W(10, 0x0FF);
    W(11, 0x1FF);

    // Route DAC audio to output mixers
    W(34, 0x100);
    W(37, 0x100);

    // Enable left and right output mixers
    W(47, 0x018);

    // Four-wire speaker volume
    W(40, 0x070);
    W(41, 0x170);

    // Headphone / AUX output volume
    W(2, 0x079);
    W(3, 0x179);

    // Enable DACs, headphones, speakers and PLL
    W(26, 0x1F9);
    sleep_ms(100);

    // Enable Class D speaker amplifiers
    W(49, 0x0F7);

    return true;
}

#undef W

// ==========================================
// I2S AUDIO OUTPUT
// ==========================================

static void setup_i2s(PIO pio, uint sm) {

    uint offset = pio_add_program(
        pio, &audio_i2s_program
    );

    pio_sm_config c =
        audio_i2s_program_get_default_config(offset);

    // Initialize I2S pins
    pio_gpio_init(pio, BCLK_PIN);
    pio_gpio_init(pio, LRCLK_PIN);
    pio_gpio_init(pio, DATA_PIN);

    // Configure clocks and data pin
    sm_config_set_sideset_pins(&c, BCLK_PIN);
    sm_config_set_out_pins(&c, DATA_PIN, 1);

    // Shift MSB first, pull 32 bits automatically
    sm_config_set_out_shift(&c, false, true, 32);

    // Join FIFO for transmission
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // 48 kHz, two 32-bit channel slots
    float divider =
        (float)clock_get_hz(clk_sys) /
        (SAMPLE_RATE * 128.0f);

    sm_config_set_clkdiv(&c, divider);

    pio_sm_init(pio, sm, offset, &c);

    pio_sm_set_consecutive_pindirs(
        pio, sm, BCLK_PIN, 3, true
    );

    // Start from the I2S entry point
    pio_sm_exec(
        pio,
        sm,
        pio_encode_jmp(
            offset + audio_i2s_offset_entry_point
        )
    );
}

// ==========================================
// PHONE AUDIO INPUT
// ==========================================

static void setup_audio_input(void) {

    adc_init();

    // GP26 = ADC0
    adc_gpio_init(AUDIO_INPUT_PIN);
    adc_select_input(AUDIO_ADC_CHANNEL);

    printf("Audio input configured on GP26\n");
    fflush(stdout);
}

// ==========================================
// MAIN PROGRAM
// ==========================================

int main(void) {

    stdio_init_all();
    sleep_ms(2000);

    printf("STARTING PHONE AUDIO PLAYBACK\n");
    fflush(stdout);

    // --------------------------------------
    // Initialize I2C
    // --------------------------------------

    i2c_init(i2c0, 100000);

    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    printf("I2C configured on GP%d/GP%d\n",
           SDA_PIN, SCL_PIN);

    fflush(stdout);

    // --------------------------------------
    // Initialize WM8960
    // --------------------------------------

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

    // --------------------------------------
    // Initialize phone audio input
    // --------------------------------------

    setup_audio_input();

    // Allow input voltage to settle
    sleep_ms(100);

    // Estimate initial DC bias
    uint32_t bias_sum = 0;

    for (int i = 0; i < 256; i++) {
        bias_sum += adc_read();
        sleep_us(20);
    }

    int32_t initial_bias = bias_sum / 256;

    printf("ADC bias: %ld\n", (long)initial_bias);
    fflush(stdout);

    // Fixed-point DC tracking
    int32_t dc_bias = initial_bias * 1024;

    // --------------------------------------
    // Initialize I2S output
    // --------------------------------------

    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);

    setup_i2s(pio, sm);

    // Queue initial silence
    pio_sm_put_blocking(pio, sm, 0);
    pio_sm_put_blocking(pio, sm, 0);

    // Start I2S
    pio_sm_set_enabled(pio, sm, true);

    printf("I2S enabled: GP10/GP11/GP12\n");
    printf("PHONE AUDIO RUNNING\n");
    fflush(stdout);

    // ======================================
    // REAL-TIME AUDIO LOOP
    // ======================================

    while (true) {

        // Read analog audio from GP26
        uint16_t raw = adc_read();

        // Slowly track the DC bias
        dc_bias +=
            ((int32_t)raw * 1024 - dc_bias) / 1024;

        // Remove DC bias to create signed audio
        int32_t centered =
            (int32_t)raw - (dc_bias / 1024);

        // Amplify audio to 16-bit range
        int32_t amplified = centered * AUDIO_GAIN;

        // Prevent overflow and clipping wraparound
        if (amplified > 32767) {
            amplified = 32767;
        }

        if (amplified < -32768) {
            amplified = -32768;
        }

        int16_t sample = (int16_t)amplified;

        // Place audio into upper 16 bits
        // of each 32-bit I2S slot
        uint32_t audio_word =
            ((uint32_t)(uint16_t)sample) << 16;

        // Duplicate mono audio to both channels
        pio_sm_put_blocking(pio, sm, audio_word);
        pio_sm_put_blocking(pio, sm, audio_word);
    }
}
