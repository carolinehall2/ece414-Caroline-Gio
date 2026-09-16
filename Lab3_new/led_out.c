#include "led_out.h"
#include "pico/stdlib.h"
static const uint32_t led_out_pins = 0x000003fc;// GPIO pins 2-9 for LED output
void led_out_init () {
    gpio_init_mask(led_out_pins);
    gpio_set_dir_out_masked(led_out_pins);
}
 

void led_out_write(uint8_t val){
    
    gpio_put_masked(led_out_pins, val << 2); // Shift value to match GPIO pins 2-9
    
}
