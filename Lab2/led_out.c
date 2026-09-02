#include "led_out.h"
#include "pico /stdlib.h"
void led _out_init () {
const uint32_t led_out_pins = 0x000003fc; // GPIO pins 2-9 for LED output
 gpio_init (led_out_pins);
 gpio_set_dir (led_out_pins, GPIO_OUT);
 
}
void led_out_write(unir8_t val){
    
    gpio_put_masked(0x000003fc, val << 2); // Shift value to match GPIO pins 2-9
    sleep_ms(100); // Delay for 100 milliseconds
}
