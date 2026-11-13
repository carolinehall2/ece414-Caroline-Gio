
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pio_instructions.h"
#include "audio_i2s.pio.h"

#define WM8960_ADDR 0x1A
#define SAMPLE_RATE 48000

#define SDA_PIN 4
#define SCL_PIN 5

#define BCLK_PIN 10
#define LRCLK_PIN 11
#define DATA_PIN 12

// Write a register to the WM8960
static bool wm_write(uint8_t reg, uint16_t val) {
    uint8_t data[2] = {
        (uint8_t)((reg << 1) | ((val >> 8) & 1)),
        (uint8_t)(val & 0xFF)
    };

    int result = i2c_write_timeout_us(
        i2c0, WM8960_ADDR,
        data, 2, false, 100000
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

// Initialize the WM8960 audio codec
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

    // Clock and audio format configuration
    W(4, 0x005);
    W(7, 0x00A);  // 24-bit I2S, normal channel order
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

    // Original four-wire speaker volume
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

// Configure PIO for I2S audio
static void setup_i2s(PIO pio, uint sm) {

    uint offset = pio_add_program(
        pio, &audio_i2s_program
    );

    pio_sm_config c =
        audio_i2s_program_get_default_config(offset);

    // Initialize GPIO pins
    pio_gpio_init(pio, BCLK_PIN);
    pio_gpio_init(pio, LRCLK_PIN);
    pio_gpio_init(pio, DATA_PIN);

    // Assign I2S clock and data pins
    sm_config_set_sideset_pins(&c, BCLK_PIN);
    sm_config_set_out_pins(&c, DATA_PIN, 1);

    // Shift MSB first, automatically pull 32 bits
    sm_config_set_out_shift(&c, false, true, 32);

    // Join FIFOs for audio transmission
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // 32-bit slots, two channels, 48 kHz
    float divider =
        (float)clock_get_hz(clk_sys) /
        (SAMPLE_RATE * 128.0f);

    sm_config_set_clkdiv(&c, divider);

    // Initialize state machine
    pio_sm_init(pio, sm, offset, &c);

    pio_sm_set_consecutive_pindirs(
        pio, sm, BCLK_PIN, 3, true
    );

    // Jump to the I2S program entry point
    pio_sm_exec(
        pio, sm,
        pio_encode_jmp(
            offset + audio_i2s_offset_entry_point
        )
    );
}

int main(void) {

    stdio_init_all();
    sleep_ms(2000);

    printf("STARTING WM8960 SPEAKER TEST\n");
    fflush(stdout);

    // Initialize I2C
    i2c_init(i2c0, 100000);

    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    printf("I2C configured on GP%d/GP%d\n",
           SDA_PIN, SCL_PIN);
    fflush(stdout);

    // Initialize WM8960
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

    // Set up I2S
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);

    setup_i2s(pio, sm);

    // Queue silence before starting
    pio_sm_put_blocking(pio, sm, 0);
    pio_sm_put_blocking(pio, sm, 0);

    // Start the PIO state machine
    pio_sm_set_enabled(pio, sm, true);

    printf("I2S enabled: GP10/GP11/GP12\n");
    printf("AUDIO RUNNING\n");
    fflush(stdout);

    // 440 Hz triangle wave
    uint32_t phase = 0;
    const uint32_t phase_step = 39370534u;

    // Alternate left and right channels
    uint32_t last_switch = time_us_32();
    bool left_channel = true;

    printf("LEFT CHANNEL TEST\n");
    fflush(stdout);

    while (true) {

        uint32_t now = time_us_32();

        // Switch channels every 3 seconds
        if (now - last_switch >= 3000000u) {

            last_switch = now;
            left_channel = !left_channel;

            printf("%s CHANNEL TEST\n",
                   left_channel ? "LEFT" : "RIGHT");
            fflush(stdout);
        }

        // Generate a 440 Hz test tone
        phase += phase_step;

        uint32_t position = phase >> 16;

        int32_t triangle;

        if (position < 32768) {
            triangle = (int32_t)position - 16384;
        } else {
            triangle = 49152 - (int32_t)position;
        }

        // Scale to a 16-bit audio sample
        int16_t sample = (int16_t)(triangle / 4);

        // Place sample in the upper bits of a 32-bit slot
        uint32_t audio =
            ((uint32_t)(uint16_t)sample) << 16;

        uint32_t left_word;
        uint32_t right_word;

        // Send audio to only one channel at a time
        if (left_channel) {
            left_word = audio;
            right_word = 0;
        } else {
            left_word = 0;
            right_word = audio;
        }

        // Send left and right 32-bit channel slots
        pio_sm_put_blocking(pio, sm, left_word);
        pio_sm_put_blocking(pio, sm, right_word);
    }
}
