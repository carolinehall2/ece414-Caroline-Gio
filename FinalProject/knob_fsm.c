
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "knob_fsm.h"
#include "system_state.h"

// Phone audio uses ADC0 (GP26)
#define PHONE_ADC 0

// Timing for knob readings
#define KNOB_INTERVAL_US 2000u
#define MUX_SETTLE_US 50u

// Global potentiometer values
uint16_t volume_val = 4095;
uint16_t bass_val = 2048;
uint16_t mid_val = 2048;
uint16_t treble_val = 2048;

// FSM states
static enum {
    SELECT,
    SETTLE,
    READ,
    UPDATE
} state;

static uint8_t channel;
static uint32_t deadline;
static uint16_t pending;

// Array pointing to each knob value
static uint16_t *const values[] = {
    &volume_val,
    &bass_val,
    &mid_val,
    &treble_val
};

// Select multiplexer channel
static void select_mux(uint8_t ch) {

    gpio_put(MUX_S0, ch & 1u);
    gpio_put(MUX_S1, (ch >> 1) & 1u);
}

// Read the selected potentiometer
static uint16_t read_mux(void) {

    // Switch ADC to potentiometers
    adc_select_input(MUX_ADC);

    // Discard first reading
    (void)adc_read();

    uint16_t value = adc_read();

    // Restore phone audio ADC
    adc_select_input(PHONE_ADC);

    // Discard first reading after switching back
    (void)adc_read();

    return value;
}

// Update shared system state
static void publish(void) {

    g_state.volume =
        (uint8_t)((volume_val * 100u + 2047u) / 4095u);

    g_state.bass =
        (uint8_t)((bass_val * 100u + 2047u) / 4095u);

    g_state.mid =
        (uint8_t)((mid_val * 100u + 2047u) / 4095u);

    g_state.treble =
        (uint8_t)((treble_val * 100u + 2047u) / 4095u);

    system_state_publish_knobs();
}

// Initialize potentiometers
void knob_fsm_init(void) {

    // ADC is already initialized by audio code

    gpio_init(MUX_S0);
    gpio_set_dir(MUX_S0, GPIO_OUT);

    gpio_init(MUX_S1);
    gpio_set_dir(MUX_S1, GPIO_OUT);

    adc_gpio_init(MUX_SIG);

    // Read all four knobs before audio starts
    for (uint8_t ch = 0; ch < 4; ++ch) {

        select_mux(ch);

        sleep_us(MUX_SETTLE_US);

        *values[ch] = read_mux();
    }

    publish();

    channel = 0;
    state = SELECT;
    deadline = time_us_32();
}

// Run one step of the knob FSM
void knob_fsm_tick(void) {

    uint32_t now = time_us_32();

    switch (state) {

        case SELECT:

            if ((int32_t)(now - deadline) < 0)
                return;

            select_mux(channel);

            deadline = now + MUX_SETTLE_US;

            state = SETTLE;

            break;

        case SETTLE:

            if ((int32_t)(now - deadline) >= 0)
                state = READ;

            break;

        case READ:

            pending = read_mux();

            state = UPDATE;

            break;

        case UPDATE:

            // Smooth the potentiometer reading
            *values[channel] =
                (uint16_t)(
                    (3u * *values[channel]
                    + pending + 2u) / 4u
                );

            publish();

            // Move to the next potentiometer
            channel = (channel + 1u) & 3u;

            deadline = now + KNOB_INTERVAL_US;

            state = SELECT;

            break;
    }
}
