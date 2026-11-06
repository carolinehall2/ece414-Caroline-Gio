#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define I2C_PORT i2c0
#define SDA_PIN 4
#define SCL_PIN 5
#define WM8960_ADDR 0x1A

int main(void) {

    stdio_init_all();

    // Initialize I2C at 100 kHz
    i2c_init(I2C_PORT, 100000);

    // Configure I2C pins
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    sleep_ms(2000);

    printf("Starting WM8960 Detection Test...\n");

    while (true) {

        // Test communication with WM8960
        uint8_t data[2] = {0x1E, 0x00};

        int result = i2c_write_timeout_us(
            I2C_PORT,
            WM8960_ADDR,
            data,
            2,
            false,
            100000
        );

        if (result == 2) {
            printf("WM8960 DETECTED at address 0x1A!\n");
        }
        else {
            printf("WM8960 NOT DETECTED (result: %d)\n", result);
        }

        sleep_ms(2000);
    }
}