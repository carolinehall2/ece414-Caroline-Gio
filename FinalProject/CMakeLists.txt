cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(dj_board C CXX ASM)

pico_sdk_init()

add_executable(dj_board)

# Generate the header for PIO SPI display routines
pico_generate_pio_header(dj_board ${CMAKE_CURRENT_LIST_DIR}/SPIPIO.pio)

target_sources(dj_board PRIVATE 
    main.c       
    ts_lcd.c        
    TFTMaster.c
    glcdfont.c
)

# Enable USB serial output, disable UART
pico_enable_stdio_usb(dj_board 1)
pico_enable_stdio_uart(dj_board 0)

target_link_libraries(dj_board PRIVATE 
    pico_stdlib 
    hardware_adc 
    hardware_gpio 
    hardware_pio 
    hardware_irq    
    pico_bootsel_via_double_reset
)

pico_add_extra_outputs(dj_board)