cmake_minimum_required(VERSION 3.13)

set(PICO_PLATFORM "rp2040")
set(PICO_BOARD "pico_w")

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/3rdparty/pico-sdk")
include(3rdparty/pico-sdk/external/pico_sdk_import.cmake)

project(bt2i2c)

pico_sdk_init()

add_subdirectory(src)
add_subdirectory(host/pico_receiver)

pico_set_program_name(bt2i2c "BT2UART Bridge")
pico_set_program_version(bt2i2c "0.2")
pico_enable_stdio_uart(bt2i2c 1)
