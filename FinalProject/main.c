
/*
 * Raspberry Pi Pico DJ Board
 * Mono AUX -> RP2040 ADC -> WM8960 speakers
 *
 * GP26: Analog audio input
 * GP4:  I2C SDA
 * GP5:  I2C SCL
 * GP10: I2S BCLK
 * GP11: I2S LRCLK
 * GP12: I2S DAC data
 *
 * Initial audio pass-through test.
 * Knob FSM temporarily disabled.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pio_instructions.h"

#include "audio_i2s.pio.h"

// Configuration
#define WM8960_ADDR     0x1A
#define SAMPLE_RATE     48000

#define ADC_GPIO        26
#define ADC_CHANNEL     0

#define I2C_SDA_PIN     4
#define I2C_SCL_PIN     5

#define I2S_BCLK_PIN    10
#define I2S_LRCLK_PIN   11
#define I2S_DATA_PIN    12

// Start with low digital gain
#define ADC_GAIN        2

// DC offset tracking
#define DC_TRACK_SHIFT  12

static int32_t dc_estimate = (2048 << DC_TRACK_SHIFT);

// ------------------------------------------------
// WM8960 Register Write
// ------------------------------------------------

static bool wm_write(uint8_t reg, uint16_t value) {

    uint8_t bytes[2] = {
        (uint8_t)((reg << 1) | ((value >> 8) & 1)),
        (uint8_t)(value & 0xFF)
    };

    int result = i2c_write_timeout_us(
        i2c0,
        WM8960_ADDR,
        bytes,
        2,
        false,
        100000
    );

    if (result != 2) {
        printf("WM8960 register %u failed: %d\n",
               reg, result);
        return false;
    }

    return true;
}

#define W(reg,val) \
    do { if (!wm_write((reg),(val))) return false; } while (0)

// ------------------------------------------------
// WM8960 Initialization
// ------------------------------------------------

static bool wm_init(void) {

    // Reset
    W(15, 0x000);
    sleep_ms(20);

    // Reference voltage and VMID
    W(25, 0x1C0);
    sleep_ms(100);
    W(25, 0x0C0);

    // Existing board PLL settings:
    // 24 MHz MCLK
    W(52, 0x038);
    W(53, 0x031);
    W(54, 0x026);
    W(55, 0x0E8);

    W(26, 0x001);
    sleep_ms(15);

    // Clocking
    W(4, 0x005);

    // 16-bit standard I2S, codec in slave mode
    W(7, 0x002);

    W(8, 0x1C0);

    // DAC controls
    W(5, 0x000);
    W(10, 0x0FF);
    W(11, 0x1FF);

    // Route DAC to output mixers
    W(34, 0x100);
    W(37, 0x100);

    // Speaker output controls
    W(47, 0x018);
    W(40, 0x070);
    W(41, 0x170);

    // Enable output stages
    W(26, 0x199);
    sleep_ms(100);

    W(49, 0x0F7);

    return true;
}

#undef W

// ------------------------------------------------
// Initialize Pico ADC
// ------------------------------------------------

static void setup_adc(void) {

    adc_init();

    adc_gpio_init(ADC_GPIO);

    adc_select_input(ADC_CHANNEL);

    // Initialize DC tracking from actual input
    int32_t sum = 0;

    for (int i = 0; i < 256; i++) {
        sum += adc_read();
    }

    dc_estimate = (sum / 256) << DC_TRACK_SHIFT;
}

// ------------------------------------------------
// Initialize PIO I2S
// ------------------------------------------------

static void setup_i2s(PIO pio, uint sm) {

    uint offset = pio_add_program(
        pio,
        &audio_i2s_swapped_program
    );

    pio_sm_config config =
        audio_i2s_swapped_program_get_default_config(offset);

    pio_gpio_init(pio, I2S_BCLK_PIN);
    pio_gpio_init(pio, I2S_LRCLK_PIN);
    pio_gpio_init(pio, I2S_DATA_PIN);

    pio_sm_set_consecutive_pindirs(
        pio,
        sm,
        I2S_BCLK_PIN,
        3,
        true
    );

    sm_config_set_sideset_pins(
        &config,
        I2S_BCLK_PIN
    );

    sm_config_set_out_pins(
        &config,
        I2S_DATA_PIN,
        1
    );

    sm_config_set_out_shift(
        &config,
        false,
        true,
        32
    );

    sm_config_set_fifo_join(
        &config,
        PIO_FIFO_JOIN_TX
    );

    float divider =
        (float)clock_get_hz(clk_sys) /
        (SAMPLE_RATE * 66.0f);

    sm_config_set_clkdiv(&config, divider);

    pio_sm_init(pio, sm, offset, &config);

    // Start at the PIO entry point, not the
    // first instruction of the bit loop.
    pio_sm_exec(
        pio,
        sm,
        pio_encode_jmp(
            offset +
            audio_i2s_swapped_offset_entry_point
        )
    );
}

// ------------------------------------------------
// Audio sample processing
// ------------------------------------------------

static inline int16_t process_audio(uint16_t raw) {

    // Slowly track the ADC DC bias.
    int32_t target =
        ((int32_t)raw << DC_TRACK_SHIFT);

    dc_estimate +=
        (target - dc_estimate) >> DC_TRACK_SHIFT;

    // Remove the DC component.
    int32_t centered =
        (target - dc_estimate) >> DC_TRACK_SHIFT;

    // Convert 12-bit magnitude to 16-bit
    // audio and apply modest digital gain.
    int32_t sample =
        centered * 16 * ADC_GAIN;

    // Clamp to prevent digital overflow.
    if (sample > 16000) sample = 16000;
    if (sample < -16000) sample = -16000;

    return (int16_t)sample;
}

// ------------------------------------------------
// Main
// ------------------------------------------------

int main(void) {

    stdio_init_all();

    sleep_ms(2000);

    printf("Starting DJ Board Audio Test...\n");

    // I2C setup
    i2c_init(i2c0, 100000);

    gpio_set_function(
        I2C_SDA_PIN,
        GPIO_FUNC_I2C
    );

    gpio_set_function(
        I2C_SCL_PIN,
        GPIO_FUNC_I2C
    );

    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // ADC setup
    setup_adc();

    // WM8960 setup
    if (!wm_init()) {
        while (true) {
            printf("WM8960 initialization FAILED\n");
            sleep_ms(2000);
        }
    }

    printf("WM8960 initialized\n");

    // I2S setup
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);

    setup_i2s(pio, sm);

    // Prime the I2S output with silence
    pio_sm_put_blocking(pio, sm, 0);
    pio_sm_put_blocking(pio, sm, 0);

    pio_sm_set_enabled(pio, sm, true);

    printf("Starting mono audio pass-through\n");
    fflush(stdout);

    // Continuous audio processing loop
    while (true) {

        // Read analog music from phone
        uint16_t raw = adc_read();

        // Process audio
        int16_t sample = process_audio(raw);

        // Duplicate mono sample to both channels
        uint16_t audio16 = (uint16_t)sample;

        uint32_t stereo =
            ((uint32_t)audio16 << 16) |
            (uint32_t)audio16;

        // Send one stereo frame
        pio_sm_put_blocking(pio, sm, stereo);
    }
}
