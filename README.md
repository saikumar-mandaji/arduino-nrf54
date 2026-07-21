# arduino-nrf54

[![CI](https://github.com/saikumar-mandaji/arduino-nrf54/actions/workflows/ci.yml/badge.svg)](https://github.com/saikumar-mandaji/arduino-nrf54/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Target](https://img.shields.io/badge/target-nRF54L15--DK-00A9CE)
![Status](https://img.shields.io/badge/status-hardware%20bring--up%20in%20progress-orange)

**An Arduino API core for the Nordic nRF54L15** -- built on Nordic's `nrfx`
HAL and ARM's CMSIS-Core, since no Arduino core exists yet for the nRF54
series. Scope so far: GPIO (`pinMode`/`digitalWrite`/`digitalRead`),
`Serial` (over UARTE20), `millis()`/`micros()`/`delay()` (over GRTC),
`SPI` (over SPIM21), `Wire`/I2C (over TWIM22), `analogRead()` (over
SAADC), `analogWrite()` (over PWM20), and `attachInterrupt()`/
`detachInterrupt()` (over GPIOTE).

**Status: open source, hardware bring-up in progress on a real
nRF54L15-DK.** `Blink` is confirmed working end-to-end on real hardware
(LED genuinely blinks, confirmed via direct GPIO register reads over
SWD, not just visually). Several real bugs have been found and fixed
via SWD debugging that no amount of compile-time checking could have
caught (see `docs/VERIFICATION.md` for the full account) -- most
significantly: **this chip's IRQ vectors weren't actually wired to this
core's interrupt handlers at all** (a missing nrfx integration include,
plus missing per-instance trampoline functions nrfx expects the
integrator to provide), which silently broke `attachInterrupt()`
entirely and `Serial`'s RX path, invisible to every build/link check
until real hardware exposed it. Fixed and reverified: `Blink`'s LED
genuinely toggles, System ON idle `delay()` is confirmed via SWD to
actually halt the CPU at the `wfi` instruction (not spin), and
`PWMFade`'s duty cycle is confirmed genuinely changing over real time.
`Wire`/`SPI` continue to build/run without crashing but still need
protocol-level hardware confirmation (see `docs/VERIFICATION.md`).
**`Serial`'s long-standing output-visibility mystery is now resolved**:
four compounding bugs (wrong UARTE instance/pins, the DK's onboard VCOM
bridge tri-stating its UART pins until DTR is asserted, EasyDMA
requiring RAM-resident TX buffers, and a missing
`nrfx_uarte_rx_enable()` call), all found by reading vendor headers/
devicetree directly and fixed -- confirmed hardware-working
bidirectionally (`SerialEcho`'s greeting received, and a round-trip
echo test both succeeded) over the DK's `COM21` VCOM port. See
`docs/VERIFICATION.md` for the full account, including one known,
already-scoped-for-v2 limitation (the single-byte RX buffer can drop
bytes under fast back-to-back input). The SDC/MPSL BLE controller
bring-up sequence is implemented and **hardware-confirmed working**
(not yet a usable BLE feature -- see Roadmap). Low-power sleep (System
ON idle) is implemented and hardware-verified; System OFF deep sleep is
not yet implemented. Contributions and hardware reports welcome.

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
4. Select **Tools > Board** and pick your board: **Nordic nRF54L15-DK**,
   **Seeed Studio XIAO nRF54L15**, **Ezurio BL54L15 DVK**, or **Raytac
   AN54LQ-DB-15**. Pick the board's serial/SWD port and upload like any
   other Arduino board.

**The toolchain is installed for you.** Boards Manager downloads the
real ARM GNU Toolchain 14.2.rel1 (`arm-none-eabi-gcc`/`g++`/`objcopy`)
automatically as a tool dependency the first time you install this
platform, the same mechanism ESP32's Arduino core uses for its own
compiler. On Linux and macOS (x86_64 and ARM64) this downloads the
official archive straight from
[developer.arm.com](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads),
checksummed (SHA-256) against a copy independently downloaded and
hashed for this project. On Windows it downloads a **repackaged mirror
hosted in this project's own GitHub Releases** instead of ARM's raw
archive -- ARM's official Windows `.zip` extracts to multiple top-level
folders instead of one, which Arduino's tool installer requires (this
was caught by actually testing a fresh install, not assumed); the
mirror is the identical, unmodified toolchain contents, just re-zipped
with a single wrapping folder. You do **not** need to install
`arm-none-eabi-gcc` yourself for this install path. Flashing real
hardware still needs Nordic's `nrfutil` separately (`winget install
NordicSemiconductor.nrfutil`, then `nrfutil install device`) -- see
`docs/hardware/BOM.md` for details.

This flow was verified end-to-end for real, with the pre-existing
manual toolchain removed from `PATH` entirely so there was no way to
silently pass by falling back to it: fresh `arduino-cli core
update-index` + `core install saikumar-mandaji:nrf54l` against a
locally-served copy of this exact release, followed by a real compile
of `Blink` against **all four supported boards** (nRF54L15-DK, XIAO
nRF54L15, Ezurio BL54L15 DVK, Raytac AN54LQ-DB-15), using only the
Boards-Manager-downloaded toolchain, and a real GitHub Release/package
index update-index round trip against the published v0.2.0 release.

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

- **BLE**: not exposed to sketches yet, but the controller bring-up
  sequence is implemented and **hardware-verified**.
  `cores/nrf54l/mpsl_glue.c` (gated behind a build flag,
  `ARDUINO_NRF54_MPSL_ENABLED`, so ordinary sketches keep building
  normally) calls `mpsl_init()` -> `sdc_init()` ->
  `sdc_rand_source_register()` (wired to this chip's real CRACEN
  hardware entropy source) -> `sdc_enable()`, with this chip's real
  RADIO/GRTC/TIMER10/clock IRQ vectors routed to MPSL's handlers --
  confirmed genuinely succeeding on a real nRF54L15-DK
  (`examples/BLEHardwareTest`), not just linking. There's still no
  Arduino API or HCI host, so none of this is reachable from a normal
  sketch yet. See [`docs/BLE_ROADMAP.md`](docs/BLE_ROADMAP.md) for status and the
  concrete next steps.
- **Low-power sleep modes**: System ON idle is implemented and
  **hardware-verified** -- `delay(ms)` sleeps via WFI + a GRTC compare
  channel instead of busy-waiting, confirmed over SWD on a real DK that
  the CPU genuinely halts at the `wfi` instruction rather than spinning.
  System OFF (deep sleep) is not implemented yet. See
  [`docs/LOW_POWER_ROADMAP.md`](docs/LOW_POWER_ROADMAP.md) for status
  and the System OFF scoping notes.
- **`Wire`/`SPI`/`analogRead`/`analogWrite`/`attachInterrupt`**: all run
  on real hardware without crashing after a critical IRQ-vector-wiring
  bug was found and fixed this session (`attachInterrupt` was completely
  non-functional before the fix, on any pin). Protocol-level correctness
  (actual I2C/SPI bus data, actual button-press-to-callback firing) still
  needs further hardware confirmation -- see `docs/VERIFICATION.md`.
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
