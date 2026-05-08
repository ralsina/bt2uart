add_executable(bt2i2c
    main.c
    bt_keyboard.c
    fifo.c
    reg.c
    interrupt.c
    i2c_slave.c
)

target_include_directories(bt2i2c PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PICO_SDK_PATH}/lib/btstack/src
    ${PICO_SDK_PATH}/lib/btstack/platform/embedded
    ${PICO_SDK_PATH}/lib/btstack
    ${PICO_SDK_PATH}/lib/btstack/3rd-party/bluedroid/encoder/include
    ${PICO_SDK_PATH}/lib/btstack/3rd-party/bluedroid/decoder/include
)

target_link_libraries(bt2i2c
    pico_stdlib
    pico_cyw43_arch_none
    pico_btstack_cyw43
    pico_btstack_ble
    hardware_i2c
)

pico_enable_stdio_uart(bt2i2c 1)
pico_enable_stdio_usb(bt2i2c 1)
pico_add_extra_outputs(bt2i2c)
