# BT2UART Bridge

BLE HID keyboard → Pico W → UART → Host.

Connects to a Bluetooth LE keyboard and forwards keystrokes over UART
as raw HID report frames. Includes a receiver firmware that decodes
and logs the key events over USB serial.

## Quick Start

```sh
cd build && cmake .. && make bt2uart pico_receiver
```

Flash `build/src/bt2uart.uf2` to Pico W (sender), and
`build/host/pico_receiver/pico_receiver.uf2` to another Pico (receiver).

Wire sender GP4 → receiver GP5, connect GND ↔ GND, open USB serial on
both. Press the keyboard's pair button — keystrokes appear on the receiver.

## Project Structure

```
src/bt_keyboard.c    BLE scan, pair, HID report handler
src/main.c           Init UART + BT stack + run loop
src/pins.h           Pin assignments (UART1 on GP4/GP5)
host/pico_receiver/  Receiver firmware (UART → USB serial)
```

## Protocol

`[0xFE] [8-byte HID report]` at 115200 baud 8N1 on UART1 (GP4 TX, GP5 RX).

See [HARDWARE.md](HARDWARE.md) for wiring, protocol details, and testing.
