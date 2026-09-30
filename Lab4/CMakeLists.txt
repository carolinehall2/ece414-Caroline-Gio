cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(Lab4)

pico_sdk_init()

add_executable(Lab4)

pico_generate_pio_header(Lab4 ${CMAKE_CURRENT_LIST_DIR}/SPIPIO.pio)

target_sources(Lab4 PRIVATE 
    ts_test.c       
    ts_lcd.c        
    TFTMaster.c
    glcdfont.c
)

target_link_libraries(Lab4 PRIVATE 
    pico_stdlib 
    hardware_adc 
    hardware_gpio 
    hardware_pio 
    hardware_irq    
    pico_bootsel_via_double_reset
)

pico_add_extra_outputs(Lab4)