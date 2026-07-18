# arduino-nrf54

[![CI](https://github.com/saikumar-mandaji/arduino-nrf54/actions/workflows/ci.yml/badge.svg)](https://github.com/saikumar-mandaji/arduino-nrf54/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Target](https://img.shields.io/badge/target-nRF54L15--DK-00A9CE)
![Status](https://img.shields.io/badge/status-hardware%20bring--up%20in%20progress-orange)

**An Arduino API core for the Nordic nRF54L15** -- built on Nordic's `nrfx`
HAL and ARM's CMSIS-Core, since no Arduino core exists yet for the nRF54
series. Scope so far: GPIO (`pinMode`/`digitalWrite`/`digitalRead`),
`Serial` (over UARTE30), `millis()`/`micros()`/`delay()` (over GRTC),
`SPI` (over SPIM21), `Wire`/I2C (over TWIM22), `analogRead()` (over
SAADC), `analogWrite()` (over PWM20), and `attachInterrupt()`/
`detachInterrupt()` (over GPIOTE).

**Status: private, hardware bring-up in progress on a real nRF54L15-DK.**
`Blink` is confirmed working end-to-end on real hardware (LED visibly
blinks at the correct rate). Multiple real bugs were found and fixed via
SWD debugging that no amount of compile-time checking could have caught
(see `docs/VERIFICATION.md` for the full account): `Serial` was on the
wrong UARTE instance; a GRTC startup call crashed with a NULL-pointer
HardFault; `nrfx_spim` was found to auto-toggle CS around every single
`SPI.transfer()` call, breaking multi-byte transactions. `Serial`'s
status is a genuine unresolved discrepancy -- a human using Nordic's own
Serial Terminal app reported seeing output, but automated tooling
(two different serial libraries) couldn't independently reproduce it.
SPI's pins are now confirmed correct (checked directly against the
DK's onboard flash chip's real wiring and a live register read), but
data exchanged with that chip still isn't correct, cause not yet
identified. `Wire`/`analogRead`/`analogWrite`/`attachInterrupt` haven't
been hardware-tested yet. Will go public once these are resolved.

## Quick start

**Clone with submodules** (this repo vendors Nordic's `nrfx` and ARM's
`CMSIS_6` as git submodules -- a plain clone will leave `extern/` empty):

```sh
git clone --recurse-submodules <this-repo-url>
# or, if already cloned without --recurse-submodules:
git submodule update --init --recursive
```

**Build via the Makefile** (requires `arm-none-eabi-gcc`/`g++`/`objcopy`/
`size` on PATH -- e.g. the [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)):

```sh
make EXAMPLE=Blink
make EXAMPLE=SerialEcho
make EXAMPLE=I2CScanner
make EXAMPLE=SPILoopback
make EXAMPLE=AnalogReadSerial
make EXAMPLE=PWMFade
make EXAMPLE=ButtonInterrupt
```

**Build via the Arduino IDE / `arduino-cli`**: place (or symlink/junction)
this repository at `<sketchbook>/hardware/<vendor>/nrf54l/` so
`arduino-cli`/the IDE picks up `boards.txt`/`platform.txt`, then:

```sh
arduino-cli compile --fqbn <vendor>:nrf54l:nrf54l15dk examples/Blink
```

**Flashing** (untested -- no hardware available during development, see
`docs/VERIFICATION.md`): either drag-and-drop the built `.hex` onto the
DK's `JLINK` USB mass-storage drive, or use `nrfjprog`/`nrfutil` via the
`tools.nrfjprog.upload.pattern` recipe in `platform.txt`.

Full hardware requirements: [`docs/hardware/BOM.md`](docs/hardware/BOM.md)
(short version: just the nRF54L15-DK and a USB-C cable -- both examples
use only onboard peripherals).

## Docs

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) -- design rationale: why
  build on `nrfx`, the secure-only/no-TrustZone-M v1 scoping decision, the
  wrapper-include technique that makes vendored `extern/nrfx` sources
  visible to Arduino IDE's core auto-discovery, and what's explicitly out
  of scope for v1.
- [`docs/VERIFICATION.md`](docs/VERIFICATION.md) -- what's actually been
  compiled/linked and verified versus what's still unconfirmed on real
  silicon.
- [`docs/hardware/BOM.md`](docs/hardware/BOM.md) -- parts list and the
  pin-mapping caveat (best-effort placeholders pending hardware bring-up).

## License

MIT for the code in this repository (see `LICENSE`). The `extern/nrfx` and
`extern/CMSIS_6` submodules carry their own licenses (BSD-3-Clause and
Apache-2.0 respectively).
