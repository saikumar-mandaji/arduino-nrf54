# Hardware: Bill of Materials & Wiring

## Bill of materials

| # | Part | Exact part / module | Qty | Typical price (2026) | Notes |
|---|------|----------------------|-----|----------------------|-------|
| 1 | Dev board | **Nordic nRF54L15-DK** (PCA10156) | 1 | $25-35 | Nordic's official dev kit for the nRF54L15 (Cortex-M33, TrustZone-M, native 802.15.4 + BLE 6.0 radio -- none of which this v1 core uses yet, see docs/VERIFICATION.md). Has an on-board J-Link debugger/programmer -- no separate programmer needed. |
| 2 | USB cable | USB-A to USB-C (data-capable, not charge-only) | 1 | $3-5 | Powers the DK and connects the onboard J-Link (for flashing) and the onboard VCOM UART (for Serial). |

**Total: ~$28-40.** No external components needed for the Blink/SerialEcho v1 examples -- both use the DK's onboard LED and onboard-J-Link-routed UART.

## Onboard peripherals used

- **LED1**: driven by `Blink.ino` via `LED_BUILTIN` (confirmed on real hardware).
- **Onboard J-Link VCOM UART**: used by `Serial` (UARTE20, P1.04/P1.05 -- confirmed working bidirectionally on real hardware, see `docs/VERIFICATION.md`'s "Serial mystery: RESOLVED" section; note the DK's VCOM bridge tri-states its UART pins until the host terminal asserts DTR).
- **Onboard MX25R6435F SPI flash chip**: SPI00 wiring (SCK=P2.01, MOSI=P2.02, MISO=P2.04, CS=P2.05, RESET=P2.00), confirmed against Zephyr's devicetree and a live SWD register read. See `docs/VERIFICATION.md` for the full bring-up account, including why a JEDEC ID read still isn't returning correct data even with confirmed-correct pins.

## Extra parts needed for the v2 (SPI/I2C) examples

| # | Part | Qty | Notes |
|---|------|-----|-------|
| 3 | Jumper wire (female-female) | 1 | **`SPILoopback.ino` requires MOSI physically jumpered to MISO** on the DK's headers -- it has no external SPI device to talk to, it loops back to itself. |
| 4 | Any I2C device (sensor breakout, EEPROM, etc.) -- optional | 1 | `I2CScanner.ino` works with nothing on the bus too (it'll just report "No I2C devices found"), but a real device is the more meaningful test. External 4.7k pull-up resistors on SDA/SCL may be needed if your device breakout doesn't already have them. The DK does have an onboard nPM1300 PMIC with an I2C interface, but its exact SDA/SCL pin wiring for this board wasn't found during this bring-up pass -- see `docs/VERIFICATION.md`. |

`PIN_SPI_SCK`/`MOSI`/`MISO`/`SS` in `variants/nrf54l15dk/pins_arduino.h`
are now confirmed (see above). `PIN_WIRE_SDA`/`SCL` are still
best-effort placeholders -- verify against the real DK before wiring
anything to them.

## Extra parts needed for the v3 (analog/PWM) examples

| # | Part | Qty | Notes |
|---|------|-----|-------|
| 5 | Potentiometer (10k) or any analog sensor, plus jumper wires -- optional | 1 | `AnalogReadSerial.ino` reads A0 (SAADC channel 0); without anything connected it'll just read whatever the floating pin picks up, which is fine for confirming the code runs but not meaningful as a real measurement. |
| 6 | LED + ~330Ω series resistor -- optional | 1 | `PWMFade.ino` drives PIN_PWM0; an LED makes the fade visible, but the sketch runs fine with nothing connected too. |

`A0`-`A7` are SAADC channel indices, not GPIO pin numbers -- see
`docs/ARCHITECTURE.md`. `PIN_PWM0`-`PIN_PWM3` are, like the SPI/I2C pins
above, best-effort placeholders.

**Important -- pin assignments are best-effort placeholders, not confirmed against real hardware.** This core was written in a development environment with no physical nRF54L15-DK available. `LED_BUILTIN`, `PIN_SERIAL_TX`, and `PIN_SERIAL_RX` in `variants/nrf54l15dk/pins_arduino.h` are reasonable guesses at the DK's actual pin routing, not values read off the schematic. **Before flashing either example, check the pin numbers there against Nordic's nRF54L15-DK Hardware/User Guide** (search "nRF54L15-DK Hardware User Guide" on Nordic's infocenter/DevZone) and correct them if they're wrong -- this is exactly the kind of thing the internal hardware bring-up pass this project is waiting on is meant to catch. See `docs/VERIFICATION.md` for the full disclosure of what has and hasn't been checked.

## Flashing

**Use `nrfutil device program` (SWD-based) -- confirmed working.** Install
via `winget install NordicSemiconductor.nrfutil`, then `nrfutil install
device`, then:

```sh
nrfutil device program --firmware path/to/firmware.hex --serial-number <SN>
nrfutil device reset --serial-number <SN> --reset-kind RESET_PIN
```

**Drag-and-drop (copying the `.hex` onto the DK's `JLINK` USB
mass-storage drive) does NOT work on at least this DK unit/interface
firmware combination** -- confirmed on real hardware: it produces a
`FAIL.TXT` on the drive reading "The currently active SWD interface does
not support MSD drag and drop." Try it if you like (it's zero-install),
but don't be surprised if it fails -- `nrfutil` is the reliable path.

See `docs/VERIFICATION.md` for the full real-hardware bring-up account,
including two real bugs (wrong UARTE instance for Serial, a NULL-pointer
crash in the GRTC startup path) that were only found by flashing to real
silicon and debugging over SWD.
