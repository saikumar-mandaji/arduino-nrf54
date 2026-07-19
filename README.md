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

**Status: open source, hardware bring-up in progress on a real
nRF54L15-DK.** `Blink` is confirmed working end-to-end on real hardware
(LED visibly blinks at the correct rate). Multiple real bugs were found
and fixed via SWD debugging that no amount of compile-time checking
could have caught (see `docs/VERIFICATION.md` for the full account):
`Serial` was on the wrong UARTE instance; a GRTC startup call crashed
with a NULL-pointer HardFault; `nrfx_spim` was found to auto-toggle CS
around every single `SPI.transfer()` call, breaking multi-byte
transactions. `Serial`'s status is a genuine unresolved discrepancy -- a
human using Nordic's own Serial Terminal app reported seeing output, but
automated tooling (two different serial libraries) couldn't
independently reproduce it. SPI's pins are now confirmed correct
(checked directly against the DK's onboard flash chip's real wiring and
a live register read), but data exchanged with that chip still isn't
correct, cause not yet identified. `Wire`/`analogRead`/`analogWrite`/
`attachInterrupt` haven't been hardware-tested yet. BLE and low-power
sleep modes are not implemented yet -- see the Roadmap section below.
Contributions and hardware reports welcome.

## Quick start

### Install via Arduino IDE Boards Manager (recommended)

Just like ESP32/ESP8266 cores, this SDK is distributed as a Boards
Manager package -- no manual cloning or submodule setup required:

1. Open Arduino IDE -> **File > Preferences**.
2. Add this URL to **Additional Boards Manager URLs** (comma-separate
   it if you already have others there):
   ```
   https://raw.githubusercontent.com/saikumar-mandaji/arduino-nrf54/main/package_saikumar-mandaji_nrf54_index.json
   ```
3. Open **Tools > Board > Boards Manager**, search for `nrf54l`, and
   install "Nordic nRF54L (arduino-nrf54)".
4. Select **Tools > Board > Nordic nRF54L15-DK**, pick the board's
   serial/SWD port, and upload like any other Arduino board.

**One thing Boards Manager does *not* install for you:** the
`arm-none-eabi-gcc`/`g++`/`objcopy` toolchain used to actually compile
sketches. Install the [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
and make sure it's on your `PATH` before building. Flashing real
hardware also needs Nordic's `nrfutil` (`winget install
NordicSemiconductor.nrfutil`, then `nrfutil install device`) -- see
`docs/hardware/BOM.md` for details.

This flow was verified end-to-end: `arduino-cli core update-index` +
`core install saikumar-mandaji:nrf54l` against the real published
package index and GitHub release, followed by a real compile
(`Blink`, 7904 bytes) against the genuinely-installed copy.

### Build from source (for contributors)

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

**Flashing**: use `nrfutil device program` -- confirmed working on real
hardware. Drag-and-drop onto the DK's `JLINK` USB mass-storage drive is
**not** reliable (fails on at least the DK unit used during bring-up,
see `docs/hardware/BOM.md`).

Full hardware requirements: [`docs/hardware/BOM.md`](docs/hardware/BOM.md)
(short version: just the nRF54L15-DK and a USB-C cable -- both examples
use only onboard peripherals).

## Roadmap

- **BLE**: not implemented yet. The nRF54L15's radio supports BLE 6.0,
  but this core doesn't expose it. Planned approach: Nordic's
  SoftDevice Controller (from `sdk-nrfxlib`) as the link-layer/controller,
  paired with an open host stack such as NimBLE -- see
  [`docs/BLE_ROADMAP.md`](docs/BLE_ROADMAP.md) for the scoping notes
  and open questions.
- **Low-power sleep modes**: not implemented yet. `delay()` currently
  busy-waits on GRTC rather than entering a real low-power sleep state
  -- see [`docs/LOW_POWER_ROADMAP.md`](docs/LOW_POWER_ROADMAP.md) for
  the scoping notes (System ON idle vs. System OFF deep sleep, the
  real nRF54L15 APIs involved, and concrete next steps).
- **`Wire`/`analogRead`/`analogWrite`/`attachInterrupt`**: implemented
  and compile-tested, but not yet exercised on real hardware.
- Genuine unresolved hardware mysteries (`Serial` tooling discrepancy,
  SPI JEDEC ID read) are tracked in `docs/VERIFICATION.md`.

Issues and PRs welcome -- see
[`docs/GUIDE.md`](docs/GUIDE.md) for the architecture overview before
diving in.

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
