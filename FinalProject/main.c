// Mono AUX input -> RP2040 ADC0 -> WM8960 stereo speakers.
// PIO left-justified 16-bit stereo, 48 kHz, GP10=BCLK, GP11=LRCLK, GP12=DACDAT.
// I2C0 GP4=SDA, GP5=SCL.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "audio_i2s.pio.h"
#include "knob_fsm.h"

#define WM8960_ADDR 0x1a
#define SAMPLE_RATE 48000
#define ADC_GPIO 26
#define ADC_CHANNEL 0
#define ADC_GAIN 80
#define DC_TRACK_SHIFT 12

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
    W(15, 0x000); // software reset
    sleep_ms(20);
    W(25, 0x1C0); // VMID fast start, enable VREF
    sleep_ms(100);
    W(25, 0x0C0); // normal VMID, VREF still enabled

    // 24 MHz MCLK -> PLL prescale /2 -> 12 MHz
    W(52, 0x038);
    W(53, 0x031);
    W(54, 0x026);
    W(55, 0x0E8);
    W(26, 0x001);
    sleep_ms(15);
    W(4,  0x005);
    W(7,  0x001);
    W(8,  0x1C0);
    W(5,  0x000);
    W(10, 0x0FF);
    W(11, 0x1FF);
    W(34, 0x100);
    W(37, 0x100);
    W(47, 0x018);
    W(40, 0x070);
    W(41, 0x170);
    W(26, 0x199);
    sleep_ms(100);
    W(49, 0x0F7);
    return true;
}
#undef W

static void setup_i2s(PIO pio, uint sm) {
    uint offset = pio_add_program(pio, &audio_i2s_swapped_program);
    pio_sm_config c = audio_i2s_swapped_program_get_default_config(offset);
    pio_gpio_init(pio, 10);
    pio_gpio_init(pio, 11);
    pio_gpio_init(pio, 12);
    pio_sm_set_consecutive_pindirs(pio, sm, 10, 3, true);
    sm_config_set_sideset_pins(&c, 10);
    sm_config_set_out_pins(&c, 12, 1);
    sm_config_set_out_shift(&c, false, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (SAMPLE_RATE * 66));
    pio_sm_init(pio, sm, offset, &c);
}

int main(void) {
    stdio_init_all();
    knob_fsm_init();

    // I2C Init for WM8960
    i2c_init(i2c0, 100000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);

    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    setup_i2s(pio, sm);

    pio_sm_put_blocking(pio, sm, 0);
    pio_sm_put_blocking(pio, sm, 0);
    pio_sm_set_enabled(pio, sm, true);

    wm_init();

    uint32_t last_tick = 0;
    uint32_t last_print = 0;

    printf("DJ Board initialized (Audio + Knob FSM).\n");

    while (1) {
        uint32_t now = time_us_32();

        // Tick Knob FSM every 10 ms
        if (now - last_tick >= 10000) {
            last_tick = now;
            knob_fsm_tick();
        }

        // Print values every 100 ms
        if (now - last_print >= 100000) {
            last_print = now;
            printf("VOL: %4d | BASS: %4d | MID: %4d | TREBLE: %4d\n",
                   volume_val, bass_val, mid_val, treble_val);
        }
    }
}