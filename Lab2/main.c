# include < stdio.h >
 # include "pico/stdlib.h"
 # include "led_out.h"
 # include "sw_in.h"

 int main () {
    stdio_init_all () ;
    led_out_init () ;
    sw_in_init () ;

 while ( true ) {
    bool s1 = sw_in_read1 () ;
    bool s2 = sw_in_read2 () ;

 if ( s1 && s2 ) {
    led_out_write (0 xFF ) ; // All LEDs ON
    } 
    else if ( s1 && ! s2 ) {
     led_out_write (0 x01 ) ; // Leftmost LED ON
    } 
    else if (! s1 && s2 ) {
        led_out_write (0 x80 ) ; // Rightmost LED ON
    }  else {
         led_out_write (0 x00 ) ; // All LEDs OFF
    }
    sleep_ms (10) ; // Small delay
 }
}
