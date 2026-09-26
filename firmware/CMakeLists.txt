cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pruefstand C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(pruefstand
    src/main.c
    src/cmd.c
    src/board.c
    src/crank.c
    src/crank_timing.c
    src/dac.c
    src/sense.c
    src/knock.c
)

pico_generate_pio_header(pruefstand ${CMAKE_CURRENT_LIST_DIR}/src/crank.pio)

target_compile_options(pruefstand PRIVATE -Wall -Wextra)

target_link_libraries(pruefstand
    pico_stdlib
    hardware_adc
    hardware_pio
    hardware_i2c
    hardware_spi
)

pico_enable_stdio_usb(pruefstand 1)
pico_enable_stdio_uart(pruefstand 0)

pico_add_extra_outputs(pruefstand)
