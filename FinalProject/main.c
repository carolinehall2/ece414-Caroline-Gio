#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define AUDIO_PIN 26
#define AUDIO_CHANNEL 0

int main() {
    stdio_init_all();

    adc_init();

    adc_gpio_init(AUDIO_PIN);
    adc_select_input(AUDIO_CHANNEL);

    sleep_ms(2000);

    printf("Starting Audio Input Test... /n");

    while(true){
        uint16_t min_value = 4095;
        uint16_t max_value = 0;

        for (int i = 0; i < 1000; i++){
            uint16_t sample = adc_read();

            if(sample < min_value){
                min_value = sample;
            }

            if(sample > max_value){
                max_value = sample;
            }
            sleep_us(125);
        }

        uint16_t audio_level = max_value - min_value;

        printf("MIN: %u | MAX: %u | AUDIO LEVEL: %u \n", min_value, max_value, audio_level);

        sleep_ms(100);
    }
}