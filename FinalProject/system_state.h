
#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdint.h>
#include <stdbool.h>

// Shared DJ board state
typedef struct {

    // Audio levels (0 to 100)
    uint8_t volume;
    uint8_t bass;
    uint8_t mid;
    uint8_t treble;

    // Touchscreen effect toggles
    bool bass_boost;
    bool bitcrusher;
    bool vocal_iso;
    bool mute;

    // Display update flags
    bool eq_dirty;
    bool fx_dirty;

} SystemState;

// Global shared state
extern volatile SystemState g_state;

// Initialize system state
void system_state_init(void);

// Publish updated knob values
void system_state_publish_knobs(void);

// Get a snapshot of all four knob values
uint32_t system_state_knob_snapshot(void);

#endif
