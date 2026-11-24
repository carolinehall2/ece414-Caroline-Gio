#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pio_instructions.h"
#include "hardware/adc.h"
#include "audio_i2s.pio.h"
#include "knob_fsm.h"
#include "system_state.h"
#include "ts_lcd.h"
#include "display_fsm.h"
#include "TFTMaster.h"

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

static bool wm_write(uint8_t reg, uint16_t val) {
    uint8_t data[2] = {
        (uint8_t)((reg << 1) | ((val >> 8) & 1)),
        (uint8_t)(val & 0xFF)
    };

    return i2c_write_timeout_us(
        i2c0, WM8960_ADDR,
        data, 2, false, 100000
    ) == 2;
}

#define W(reg, val) \
    do { if (!wm_write(reg, val)) return false; } while (0)

static bool wm_init(void) {
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

    W(4, 0x005);
    W(7, 0x002);
    W(8, 0x1C0);

    W(5, 0x000);
    W(10, 0x0FF);
    W(11, 0x1FF);

    W(34, 0x100);
    W(37, 0x100);
    W(47, 0x018);

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

static void setup_i2s(PIO pio, uint sm) {
    uint offset = pio_add_program(
        pio, &audio_i2s_program
    );

    pio_sm_config c =
        audio_i2s_program_get_default_config(offset);

    pio_gpio_init(pio, BCLK_PIN);
    pio_gpio_init(pio, LRCLK_PIN);
    pio_gpio_init(pio, DATA_PIN);

    sm_config_set_sideset_pins(&c, BCLK_PIN);
    sm_config_set_out_pins(&c, DATA_PIN, 1);
    sm_config_set_out_shift(&c, false, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    float divider =
        (float)clock_get_hz(clk_sys) /
        (SAMPLE_RATE * 64.0f);

    sm_config_set_clkdiv(&c, divider);

    pio_sm_init(pio, sm, offset, &c);

    pio_sm_set_consecutive_pindirs(
        pio, sm, BCLK_PIN, 3, true
    );

    pio_sm_set_pins(pio, sm, 0);

    pio_sm_exec(
        pio, sm,
        pio_encode_jmp(
            offset + audio_i2s_offset_entry_point
        )
    );
}

static void setup_audio_input(void) {
    adc_init();
    adc_gpio_init(AUDIO_INPUT_PIN);
    adc_select_input(AUDIO_ADC_CHANNEL);
}

static void toggle_effect(uint8_t effect) {
    switch (effect) {
        case 0:
            g_state.bass_boost = !g_state.bass_boost;
            break;

        case 1:
            g_state.bitcrusher = !g_state.bitcrusher;
            break;

        case 2:
            g_state.vocal_iso = !g_state.vocal_iso;
            break;

        case 3:
            g_state.mute = !g_state.mute;
            break;

        default:
            return;
    }

    g_state.fx_dirty = true;
}

static void update_effect_indicators(void) {
    static bool previous[4] = {
        false, false, false, false
    };

    bool current[4] = {
        g_state.bass_boost,
        g_state.bitcrusher,
        g_state.vocal_iso,
        g_state.mute
    };

    const int positions[4] = {
        15, 90, 165, 240
    };

    for (int i = 0; i < 4; i++) {
        if (current[i] != previous[i]) {
            uint16_t color = current[i]
                ? ILI9340_WHITE
                : ILI9340_BLACK;

            tft_drawRect(
                positions[i] + 2,
                182,
                61,
                36,
                color
            );

            previous[i] = current[i];
        }
    }
}

static void display_core(void) {
    display_fsm_init();

    uint32_t last_display = time_us_32();

    while (true) {
        uint32_t now = time_us_32();

        if ((uint32_t)(now - last_display) >= 33333u) {
            last_display = now;
            display_fsm_tick();
            update_effect_indicators();
        }

        sleep_ms(1);
    }
}

int main(void) {
    i2c_init(i2c0, 100000);

    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    if (!wm_init()) {
        while (true) {
            sleep_ms(2000);
        }
    }

    setup_audio_input();
    system_state_init();
    knob_fsm_init();
    ts_lcd_init();

    adc_select_input(AUDIO_ADC_CHANNEL);
    sleep_ms(100);

    uint32_t bias_sum = 0;

    for (int i = 0; i < 256; i++) {
        bias_sum += adc_read();
        sleep_us(20);
    }

    int32_t initial_bias = bias_sum / 256;
    int32_t dc_bias = initial_bias * 1024;

    PIO pio = pio1;
    uint sm = pio_claim_unused_sm(pio, true);

    setup_i2s(pio, sm);

    pio_sm_put_blocking(pio, sm, 0);
    pio_sm_put_blocking(pio, sm, 0);

    pio_sm_set_enabled(pio, sm, true);

    multicore_launch_core1(display_core);

    int32_t bass_low = 0;

    uint32_t last_touch = time_us_32();
    uint32_t last_press = 0;

    bool touch_was_down = false;

    uint16_t touch_x = 0;
    uint16_t touch_y = 0;

    while (true) {
        uint16_t raw = adc_read();

        dc_bias +=
            ((int32_t)raw * 1024 - dc_bias) / 1024;

        int32_t centered =
            (int32_t)raw - (dc_bias / 1024);

        bass_low +=
            ((centered * 256) - bass_low) / 22;

        int32_t low = bass_low / 256;

        int32_t bass_adjust =
            (int32_t)bass_val - 2048;

        int32_t bass_processed;

        if (bass_adjust >= 0) {
            bass_processed =
                centered +
                (low * bass_adjust * 3) / 2048;
        } else {
            bass_processed =
                centered +
                (low * bass_adjust) / 2048;
        }

        if (g_state.bass_boost) {
            bass_processed += low;
        }

        int32_t amplified =
            bass_processed * AUDIO_GAIN;

        amplified =
            (int32_t)(
                ((int64_t)amplified * volume_val)
                / 4095
            );

        if (g_state.bitcrusher) {
            amplified =
                (amplified / 2048) * 2048;
        }

        if (g_state.mute) {
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

        uint32_t stereo =
            ((uint32_t)audio16 << 16) |
            (uint32_t)audio16;

        pio_sm_put_blocking(pio, sm, stereo);

        knob_fsm_tick();

        uint32_t now = time_us_32();

        if ((uint32_t)(now - last_touch) >= 50000u) {
            last_touch = now;

            bool pressed =
                get_ts_lcd(&touch_x, &touch_y);

            knob_fsm_restart_scan();

            if (pressed &&
                !touch_was_down &&
                (uint32_t)(now - last_press) >= 300000u) {

                uint16_t screen_x = touch_y;
                int32_t screen_y = 240 - (int32_t)touch_x;

                if (screen_y >= 170 && screen_y <= 230) {
                    if (screen_x >= 15 && screen_x <= 80) {
                        toggle_effect(0);
                        last_press = now;
                    }
                    else if (screen_x >= 90 && screen_x <= 155) {
                        toggle_effect(1);
                        last_press = now;
                    }
                    else if (screen_x >= 165 && screen_x <= 230) {
                        toggle_effect(2);
                        last_press = now;
                    }
                    else if (screen_x >= 240 && screen_x <= 305) {
                        toggle_effect(3);
                        last_press = now;
                    }
                }
            }

            touch_was_down = pressed;
        }
    }
}
