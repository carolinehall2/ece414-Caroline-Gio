#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    // Audio levels (normalized 0 to 100)
    uint8_t volume;
    uint8_t bass;
    uint8_t mid;
    uint8_t treble;

    // Effect toggles
    bool bass_boost;
    bool bitcrusher;
    bool vocal_iso;
    bool mute;

    // Dirty flags to prevent redundant display drawing
    bool eq_dirty;
    bool fx_dirty;
} SystemState;

// Global shared state instance
extern volatile SystemState g_state;

void system_state_init(void);

#endif // SYSTEM_STATE_H