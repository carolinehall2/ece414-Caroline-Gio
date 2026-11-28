#include "system_state.h"

// Create the shared system state
volatile SystemState g_state;

// Packed copy of all four knob values
static uint32_t knob_snapshot;

// Initialize the DJ board settings
void system_state_init(void) {
    g_state = (SystemState) {.volume = 100,.bass = 50,.mid = 50,.treble = 50};
    system_state_publish_knobs();
}

// Publish all four knob values together
void system_state_publish_knobs(void) {
    uint32_t packed =(uint32_t)g_state.volume | ((uint32_t)g_state.bass << 8) | ((uint32_t)g_state.mid << 16)| ((uint32_t)g_state.treble << 24);
    __atomic_store_n( &knob_snapshot,packed, __ATOMIC_RELAXED);
}

// Read the current knob values
uint32_t system_state_knob_snapshot(void) {

    return __atomic_load_n(&knob_snapshot, __ATOMIC_RELAXED);
}
