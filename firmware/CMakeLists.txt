cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pruefstand C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(pruefstand
    src/main.c
    src/cmd.c
    src/board.c
    src/capture.c
    src/capture_core.c
    src/crank.c
    src/crank_timing.c
    src/dac.c
    src/dac_codec.c
    src/ecu.c
    src/sense.c
    src/sense_codec.c
    src/knock.c
    src/knock_codec.c
    src/gfx.c
    src/oled.c
    src/ui.c
    src/ui_core.c
)

pico_generate_pio_header(pruefstand ${CMAKE_CURRENT_LIST_DIR}/src/crank.pio)

target_compile_options(pruefstand PRIVATE -Wall -Wextra)

# Run entirely from RAM (the image is ~50 KB of 264 KB): no flash-cache misses
# adding jitter to the crank refill, capture timestamps and knock bursts.
pico_set_binary_type(pruefstand copy_to_ram)

target_link_libraries(pruefstand
    pico_stdlib
    hardware_adc
    hardware_pio
    hardware_i2c
    hardware_spi
    hardware_watchdog
)

pico_enable_stdio_usb(pruefstand 1)
pico_enable_stdio_uart(pruefstand 0)

pico_add_extra_outputs(pruefstand)
