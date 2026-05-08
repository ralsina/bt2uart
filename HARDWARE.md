# BT2UART Bridge

A Pico W that connects to a BLE HID keyboard and forwards keystrokes
over UART to a host (e.g. another Pico, ESP8266, etc.).

## How It Works

```
BLE Keyboard  ──[Bluetooth LE]──>  Pico W (bt2i2c)
                                      │
                                      │ UART1 (GP4 TX, GP5 RX, 115200 baud)
                                      │ Frame: [0xFE] [8-byte HID report]
                                      ▼
                                  Host (e.g. Pico Receiver)
                                      │
                                      │ USB serial
                                      ▼
                              Your computer (minicom/putty)
```

## Wiring

### Sender (Pico W running `bt2i2c`)

| Sender Pico W  | Connect to       | Pin |
|----------------|------------------|-----|
| GP4 (UART1 TX) | Host RX          | 6   |
| GP5 (UART1 RX) | Host TX (opt.)   | 7   |
| GND            | Host GND         | 38  |
| USB            | Computer (debug) |     |

### Receiver (Pico running `pico_receiver`)

| Receiver Pico  | Connect to       | Pin |
|----------------|------------------|-----|
| GP5 (UART1 RX) | Sender TX (GP4)  | 7   |
| GND            | Sender GND       | 38  |
| USB            | Computer (log)   |     |

**Minimal connection**: sender GP4 → receiver GP5 + common GND (2 wires).

Both Picos can be powered and debugged via their own USB cables simultaneously.

## Building & Flashing

### Sender
```sh
cd build && cmake .. && make bt2i2c
# Flash build/src/bt2i2c.uf2 to Pico W
```

### Receiver
```sh
cd build && cmake .. && make pico_receiver
# Flash build/host/pico_receiver/pico_receiver.uf2 to Pico
```

## UART Protocol

Each HID report from the keyboard is sent as a 9-byte frame:

```
Byte 0:     0xFE  (sync marker)
Byte 1:     Modifier bits (LCtrl=0x01, LShift=0x02, LAlt=0x04, LGui=0x08,
                           RCtrl=0x10, RShift=0x20, RAlt=0x40, RGui=0x80)
Byte 2:     Reserved (always 0)
Bytes 3-8:  Key codes (up to 6 simultaneous keys, 0 = empty)
```

115200 baud, 8N1. The receiver syncs on 0xFE — if a byte is lost, the
next 0xFE re-syncs (no checksum needed).

## Testing

1. Flash both Picos with their respective firmware
2. Wire sender GP4 → receiver GP5 + GND ↔ GND
3. Connect receiver USB to computer, open serial monitor (115200 baud)
4. Connect sender USB to computer, open second serial monitor for debug
5. The sender scans for BLE HID keyboards — press the keyboard's pair button
6. After pairing, keystrokes appear on the receiver's serial output:
   ```
   RAW: 00 00 04 00 00 00 00 00
   KEY press: A
   RAW: 00 00 00 00 00 00 00 00
   KEY release: A
   ```

## Notes

- **GP0 conflict**: Pico W's CYW43 uses GP0 for SPI — do not use GP0/GP1
  for UART. GP4/GP5 are safe.
- **Power**: Each Pico powered via its own USB. Do not connect VSYS to host 3.3V.
- **Baud rate**: 115200 — change `UART_BAUD` in `src/pins.h` if needed.
