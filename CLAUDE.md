# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

BT2UART Bridge is a Raspberry Pi Pico W firmware that connects to BLE HID keyboards and forwards keystrokes over UART to a host system. The project consists of two firmware binaries:

- **bt2uart** (sender): Runs on Pico W, handles BLE keyboard connection and HID report forwarding
- **pico_receiver** (receiver): Runs on another Pico, receives UART frames and outputs via USB serial

## Build System

This project uses CMake with the Raspberry Pi Pico SDK. The SDK is located at `3rdparty/pico-sdk/`.

### Build Commands

```bash
# Configure and build both binaries
cd build && cmake .. && make bt2uart pico_receiver

# Build only the sender (Pico W firmware)
make bt2uart

# Build only the receiver firmware
make pico_receiver
```

### Build Artifacts

- `build/src/bt2uart.uf2` - Flash to Pico W (sender)
- `build/host/pico_receiver/pico_receiver.uf2` - Flash to Pico (receiver)

### Development Workflow

Always build both binaries to ensure changes don't break either firmware:
```bash
cd build && cmake .. && make bt2uart pico_receiver
```

## Architecture

### Sender (bt2uart) Components

- **src/main.c**: Initialization and main event loop using btstack_run_loop_execute()
- **src/bt_keyboard.c**: BLE stack integration, HID device scanning, pairing, and HID report handling
- **src/usb_hid.c**: USB HID device functionality (appears as keyboard to host)
- **src/usb_descriptors.c**: USB HID report descriptors

The sender uses btstack for BLE functionality and runs the main application logic in the btstack event loop. Two timers are used:
- Heartbeat timer (5s): Blink LED and print "alive" message
- USB timer (5ms): Process USB HID tasks

### Receiver (pico_receiver) Components

- **host/pico_receiver/receiver.c**: UART frame receiver and USB serial output

### Communication Protocol

UART frames sent from sender to receiver:
- **Format**: `[0xFE] [8-byte HID report]`
- **Speed**: 115200 baud, 8N1
- **Pins**: UART1 on GP4 (TX) and GP5 (RX)

The 0xFE sync marker allows the receiver to resync if bytes are lost.

## Hardware Constraints

- **GP0/GP1 conflict**: Pico W's CYW43 chip uses GP0/GP1 for SPI - do not use these pins
- **UART pins**: GP4 (TX) and GP5 (RX) are used for UART1
- **LED**: On-board LED controlled via CYW43_WL_GPIO_LED_PIN

## Pico SDK Integration

This project uses a vendored Pico SDK in `3rdparty/pico-sdk/`. The SDK is included via:
```cmake
set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/3rdparty/pico-sdk")
include(3rdparty/pico-sdk/external/pico_sdk_import.cmake)
```

Key libraries used:
- `pico_stdlib`: Standard Pico library
- `pico_cyw43_arch_none`: CYW43 driver (non-threaded)
- `pico_btstack_cyw43`: btstack integration for CYW43
- `pico_btstack_ble`: BLE support
- `tinyusb_device`: USB device stack
- `hardware_uart`: UART driver (receiver only)

## BLE Stack (btstack)

The project uses btstack for BLE functionality. Important files:
- `src/btstack_config.h`: btstack configuration
- `src/bt_keyboard.c`: BLE HID client implementation

btstack entry point is `btstack_main()` called from main(). The application runs entirely within the btstack event loop (`btstack_run_loop_execute()`).

## Testing

For BLE keyboard functionality, you'll need:
1. Two Pico boards (one Pico W for sender, one Pico for receiver)
2. A BLE HID keyboard
3. UART connection between the Picos (GP4→GP5 + GND)
4. USB connections for debugging/flashing

See HARDWARE.md for detailed wiring and testing procedures.
