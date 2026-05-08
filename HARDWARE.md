# Hardware Wiring

## Pico W to I2C Host

```
Host I2C Port          Pico W         Pico W Pin Function
──────────────────────────────────────────────────────────
3.3V (VCC)      ─────  Pin 39 (VSYS)  Power input (via SMPS)
GND             ─────  Pin 38 (GND)   Common ground
SDA             ─────  Pin 1  (GP0)   I2C data
SCL             ─────  Pin 2  (GP1)   I2C clock
```

## Notes

- **Power**: The Pico W is powered from the host's 3.3V rail via VSYS (Pin 39).
  VSYS feeds the onboard SMPS regulator which handles BLE TX current bursts (~200mA).
- **USB**: USB can be connected simultaneously for serial debug. The VSYS and USB
  power paths are diode-isolated — the Pico draws from whichever source is active.
- **Ground**: A common ground connection is required for correct I2C voltage levels.
- **I2C pull-ups**: Pull-up resistors are enabled internally via software
  (`gpio_pull_up`). Ensure the host does the same or provides external pull-ups.
- **INT pin (GP2)**: Not connected — the host does not have an interrupt input.
  The host must poll `REG_ID_KEY` to check for available keystrokes.
- **I2C slave address**: Default is `0x1F`, configurable via `REG_ID_ADR` (0x12).

## I2C Protocol (i2c-puppet)

The Pico acts as an I2C slave implementing the i2c-puppet register protocol.

### Read a register
```
Master: [START] [0x1F << 1 | W] [reg_addr] [STOP]
Master: [START] [0x1F << 1 | R] [data_byte] [STOP]
```

### Write a register
```
Master: [START] [0x1F << 1 | W] [reg_addr | 0x80] [data_byte] [STOP]
```

The MSB of the register address acts as a write flag (0x80 = write, 0x00 = read).

### Key Registers

| Address | Name | Access | Description |
|---------|------|--------|-------------|
| 0x01 | VER | R | Version (0x10 = v1.0) |
| 0x02 | CFG | R/W | Configuration flags |
| 0x03 | INT | R/W | Interrupt flags |
| 0x04 | KEY | R | Key FIFO status (count + flags) |
| 0x09 | FIF | R | Dequeue keystroke (2 bytes: state, key) |
| 0x12 | ADR | R/W | I2C slave address |

### KEY register (0x04)
```
Bit 0-4: FIFO count (0-31)
Bit 5:   Capslock active
Bit 6:   Numlock active
```

### FIF register (0x09) — read returns 2 bytes
```
Byte 0: key state (0=idle, 1=pressed, 2=hold, 3=released)
Byte 1: key code (ASCII for regular keys, 0x9A-0x9D for modifiers)
```
