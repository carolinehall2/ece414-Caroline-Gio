#include "pico/stdlib.h"
#include "TFTMaster.h"
#include "knob_fsm.h"
#include "display_fsm.h"

// Effect status flags
int bass_boost_on = 0;
int bitcrusher_on = 0;
int vocal_iso_on  = 0;
int mute_on       = 0;

static int old_bass_h = 0;
static int old_mid_h = 0;
static int old_treble_h = 0;

void display_fsm_init(void) {
    tft_init_hw(); // from lab5
    tft_begin();
    tft_setRotation(1); // rotate 90 degrees clockwise
    tft_fillScreen(ILI9340_BLACK);

    //  header
    tft_setTextColor(ILI9340_WHITE);
    tft_setTextSize(2);
    tft_setCursor(70, 10);
    tft_writeString("DJ BOARD");

    // Outlines for 3 EQ Bars
    tft_drawRect(50, 40, 30, 100, ILI9340_WHITE);
    tft_drawRect(145, 40, 30, 100, ILI9340_WHITE);
    tft_drawRect(240, 40, 30, 100, ILI9340_WHITE);

    tft_setTextSize(1);
    tft_setCursor(52, 145);  tft_writeString("BASS");
    tft_setCursor(150, 145); tft_writeString("MID");
    tft_setCursor(240, 145); tft_writeString("TREBLE");

    // Divider Line
    tft_drawFastHLine(10, 165, 300, ILI9340_WHITE);

    // Static Outlines for 4 Effect Buttons
    tft_drawRect(15,  180, 65, 40, ILI9340_RED);
    tft_drawRect(90,  180, 65, 40, ILI9340_BLUE);
    tft_drawRect(165, 180, 65, 40, ILI9340_GREEN);
    tft_drawRect(240, 180, 65, 40, ILI9340_MAGENTA);

    tft_setCursor(20, 195);  tft_writeString("BASS+");
    tft_setCursor(95, 195);  tft_writeString("CRUSH");
    tft_setCursor(170, 195); tft_writeString("VOCAL");
    tft_setCursor(250, 195); tft_writeString("MUTE");
}

void display_fsm_tick(void) {
    // Map 0-4095 ADC values to 0-98 pixel height
    int new_bass_h   = (bass_val * 98) / 4095;
    int new_mid_h    = (mid_val * 98) / 4095;
    int new_treble_h = (treble_val * 98) / 4095;

    // Update Bass Bar if changed
    if (new_bass_h != old_bass_h) {
        tft_fillRect(51, 41, 28, 98 - new_bass_h, ILI9340_BLACK); // Clear top
        tft_fillRect(51, 41 + (98 - new_bass_h), 28, new_bass_h, ILI9340_CYAN); // Fill bar
        old_bass_h = new_bass_h;
    }

    // Update Mid Bar
    if (new_mid_h != old_mid_h) {
        tft_fillRect(146, 41, 28, 98 - new_mid_h, ILI9340_BLACK);
        tft_fillRect(146, 41 + (98 - new_mid_h), 28, new_mid_h, ILI9340_GREEN);
        old_mid_h = new_mid_h;
    }

    // Update Treble Bar
    if (new_treble_h != old_treble_h) {
        tft_fillRect(241, 41, 28, 98 - new_treble_h, ILI9340_BLACK);
        tft_fillRect(241, 41 + (98 - new_treble_h), 28, new_treble_h, ILI9340_YELLOW);
        old_treble_h = new_treble_h;
    }
}