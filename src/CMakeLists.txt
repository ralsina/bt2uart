add_executable(bt2uart
    main.c
    bt_keyboard.c
)

target_include_directories(bt2uart PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PICO_SDK_PATH}/lib/btstack/src
    ${PICO_SDK_PATH}/lib/btstack/platform/embedded
    ${PICO_SDK_PATH}/lib/btstack
    ${PICO_SDK_PATH}/lib/btstack/3rd-party/bluedroid/encoder/include
    ${PICO_SDK_PATH}/lib/btstack/3rd-party/bluedroid/decoder/include
)

target_link_libraries(bt2uart
    pico_stdlib
    pico_cyw43_arch_none
    pico_btstack_cyw43
    pico_btstack_ble
    hardware_uart
)

pico_enable_stdio_uart(bt2uart 1)
pico_enable_stdio_usb(bt2uart 1)
pico_add_extra_outputs(bt2uart)
