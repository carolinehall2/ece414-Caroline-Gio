cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(Lab5)

pico_sdk_init()

add_executable(Lab5)

pico_generate_pio_header(Lab5 ${CMAKE_CURRENT_LIST_DIR}/SPIPIO.pio)

target_sources(Lab5 PRIVATE 
    main.c       
    ts_lcd.c        
    TFTMaster.c
    glcdfont.c
)

target_link_libraries(Lab5 PRIVATE 
    pico_stdlib 
    hardware_adc 
    hardware_gpio 
    hardware_pio 
    hardware_irq    
    pico_bootsel_via_double_reset
)

pico_add_extra_outputs(Lab5)