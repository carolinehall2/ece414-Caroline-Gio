cmake_minimum_required (VERSION 3.13)

 include (pico_sdk_import.cmake)

 project (Lab3)

 pico_sdk_init ()

 add_executable ( Lab3
    main.c
    led_out.c
    sw_in.c
debounce_sw1.c
led25_out.c
timer.c
    )

 target_link_libraries (
    Lab3
    pico_stdlib
    pico_bootsel_via_double_reset
    )

 pico_add_extra_outputs( Lab3 )