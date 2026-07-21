# arduino-nrf54

[![CI](https://github.com/saikumar-mandaji/arduino-nrf54/actions/workflows/ci.yml/badge.svg)](https://github.com/saikumar-mandaji/arduino-nrf54/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Target](https://img.shields.io/badge/target-nRF54L15-00A9CE)
![Status](https://img.shields.io/badge/status-hardware%20bring--up%20in%20progress-orange)

An Arduino core for the Nordic nRF54L15 — built on Nordic's `nrfx` HAL and
ARM's CMSIS-Core, since no Arduino core exists yet for the nRF54 series.
Write ordinary `setup()`/`loop()` sketches with `Serial`, `SPI`, `Wire`,
`analogRead()`/`analogWrite()`, and `attachInterrupt()`, exactly like any
other Arduino board — no vendor SDK, no Zephyr, no RTOS required.

*Status: open source, hardware bring-up in progress on a real
nRF54L15-DK. Most peripherals below are confirmed working on real
silicon via SWD debugging, not just compiled — see [Verified on real
hardware](#verified-on-real-hardware). Contributions and hardware
reports from other supported boards are very welcome.*

# Table of contents
* [Supported peripherals](#supported-peripherals)
* [Supported boards](#supported-boards)
* **[How to install](#how-to-install)**
  - [Boards Manager Installation](#boards-manager-installation)
  - [Build from source](#build-from-source)
* **[Getting started](#getting-started)**
* [Pin macros](#pin-macros)
* [Flashing](#flashing)
* [Verified on real hardware](#verified-on-real-hardware)
* [Roadmap](#roadmap)
* [Docs](#docs)
* [License](#license)

## Supported peripherals

| API | Peripheral | Status |
|---|---|---|
| `pinMode` / `digitalWrite` / `digitalRead` | GPIO | ✅ Hardware-verified |
| `millis()` / `micros()` / `delay()` | GRTC | ✅ Hardware-verified (`delay()` sleeps via WFI, confirmed over SWD) |
| `Serial` | UARTE20 | ✅ Hardware-verified, TX + RX bidirectional |
| `SPI` | SPIM21 | ⚠️ Builds/runs; protocol-level data not yet scope-confirmed |
| `Wire` (I2C) | TWIM22 | ⚠️ Builds/runs; protocol-level data not yet scope-confirmed |
| `analogRead()` | SAADC | ⚠️ Builds/runs; not yet confirmed against a known analog source |
| `analogWrite()` | PWM20 | ✅ Hardware-verified, duty cycle confirmed changing over real time |
| `attachInterrupt()` / `detachInterrupt()` | GPIOTE | ✅ IRQ line confirmed armed; ISR-on-button-press not yet confirmed |
| BLE controller bring-up (`mpsl_init`→`sdc_enable`) | RADIO / MPSL / SDC | ✅ Hardware-verified (not yet exposed to sketches — see [Roadmap](#roadmap)) |
| System OFF deep sleep | REGULATORS | ❌ Not implemented yet |

See [`docs/VERIFICATION.md`](docs/VERIFICATION.md) for exactly how each ✅
was verified (register reads, SWD halts, measured values) — nothing here
is marked verified on the strength of "it compiled."

## Supported boards

| Board | FQBN target | Notes |
|---|---|---|
| Nordic nRF54L15-DK | `nrf54l15dk` | Primary bring-up board — the one with a physical unit in hand |
| Seeed Studio XIAO nRF54L15 | `xiao_nrf54l15` | Compiles for all examples; pin mapping cross-checked against Zephyr's devicetree, not yet hardware-tested |
| Ezurio BL54L15 DVK | `ezurio_bl54l15dvk` | Same caveat as XIAO |
| Raytac AN54LQ-DB-15 | `raytac_an54lq15db` | Same caveat as XIAO |

Full hardware requirements (BOM): [`docs/hardware/BOM.md`](docs/hardware/BOM.md)
— short version, just the board and a USB-C cable, every example uses
only onboard peripherals.

## How to install

#### Boards Manager Installation

Just like ESP32/ESP8266 cores, this SDK is distributed as a Boards
Manager package — no manual cloning or submodule setup required:

* Open the Arduino IDE.
* Open the **File > Preferences** menu item.
* Enter the following URL in **Additional Boards Manager URLs**:
  `https://raw.githubusercontent.com/saikumar-mandaji/arduino-nrf54/main/package_saikumar-mandaji_nrf54_index.json`
* Open the **Tools > Board > Boards Manager...** menu item.
* Wait for the platform indexes to finish downloading.
* Scroll down until you see the **Nordic nRF54L (arduino-nrf54)** entry and click on it.
* Click **Install**.
* After installation completes, close the **Boards Manager** window.
* Open **Tools > Board** and pick your board (see [Supported boards](#supported-boards)).

**The toolchain is installed for you.** Boards Manager downloads the
real ARM GNU Toolchain 14.2.rel1 (`arm-none-eabi-gcc`/`g++`/`objcopy`)
automatically as a tool dependency the first time you install this
platform — the same mechanism ESP32's Arduino core uses for its own
compiler. On Linux and macOS (x86_64 and ARM64) this downloads the
official archive straight from
[developer.arm.com](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads),
checksummed (SHA-256) against a copy independently downloaded and
hashed for this project. On Windows it downloads a **repackaged mirror
hosted in this project's own GitHub Releases** instead of ARM's raw
archive — ARM's official Windows `.zip` extracts to multiple top-level
folders instead of one, which Arduino's tool installer requires (this
was caught by actually testing a fresh install, not assumed); the
mirror is the identical, unmodified toolchain contents, just re-zipped
with a single wrapping folder. You do **not** need to install
`arm-none-eabi-gcc` yourself for this install path. Flashing real
hardware still needs Nordic's `nrfutil` separately — see
[Flashing](#flashing) below.

<details>
<summary>How this install path was verified (click to expand)</summary>

Verified end-to-end for real, with the pre-existing manual toolchain
removed from `PATH` entirely so there was no way to silently fall back
to it: fresh `arduino-cli core update-index` + `core install
saikumar-mandaji:nrf54l` against a locally-served copy of this exact
release, followed by a real compile of `Blink` against **all four
supported boards**, using only the Boards-Manager-downloaded toolchain,
and a real GitHub Release/package index update-index round trip against
the published v0.2.0 release.

</details>

#### Build from source

For contributors. Clone with submodules (this repo vendors Nordic's
`nrfx` and ARM's `CMSIS_6` as git submodules — a plain clone leaves
`extern/` empty):

```sh
git clone --recurse-submodules <this-repo-url>
# or, if already cloned without --recurse-submodules:
git submodule update --init --recursive
```

Build via the Makefile (requires `arm-none-eabi-gcc`/`g++`/`objcopy`/
`size` on `PATH` — e.g. the [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)):

```sh
make EXAMPLE=Blink
make EXAMPLE=SerialEcho
make EXAMPLE=I2CScanner
make EXAMPLE=SPILoopback
make EXAMPLE=AnalogReadSerial
make EXAMPLE=PWMFade
make EXAMPLE=ButtonInterrupt
```

Or build via the Arduino IDE / `arduino-cli`: place (or symlink/junction)
this repository at `<sketchbook>/hardware/<vendor>/nrf54l/` so
`arduino-cli`/the IDE picks up `boards.txt`/`platform.txt`, then:

```sh
arduino-cli compile --fqbn <vendor>:nrf54l:nrf54l15dk examples/Blink
```

## Getting started

* Plug in your board (see [Supported boards](#supported-boards)) via USB-C.
* Open **Tools > Board**, select **Nordic nRF54L (arduino-nrf54)**, then pick your specific board.
* Select the correct port under **Tools > Port**.
* Open one of the bundled examples (**File > Examples > Nordic nRF54L**) — `Blink` is the quickest sanity check — and hit **Upload**.

A minimal sketch looks exactly like any other Arduino target:

```cpp
#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("tick");
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
}
```

More complete examples, one per peripheral, live in
[`examples/`](examples/): `Blink`, `SerialEcho`, `I2CScanner`,
`SPILoopback`, `AnalogReadSerial`, `PWMFade`, `ButtonInterrupt`, and
`BLEHardwareTest` (controller-only, not yet a usable BLE API — see
[Roadmap](#roadmap)).

## Pin macros

You don't have to think in raw GPIO numbers. Each variant's
`pins_arduino.h` defines `P0_PIN(n)`/`P1_PIN(n)`/`P2_PIN(n)` macros that
map directly onto the nRF54L15's three physical GPIO ports:

```cpp
// Use P1_PIN(4) to refer to physical pin P1.04 directly
digitalWrite(P1_PIN(4), HIGH);

// Equivalent to using the board's linear Arduino pin numbering
digitalWrite(D11, HIGH);
```

Board-specific constants (`LED_BUILTIN`, `BTN1`, `PIN_SERIAL_TX`/`RX`,
`PIN_SPI_*`, `PIN_WIRE_*`, `PIN_PWM0..3`) are all just `P0_PIN`/`P1_PIN`/
`P2_PIN` expressions under the hood — see your board's
`variants/<board>/pins_arduino.h` for the exact mapping, and
[`docs/hardware/BOM.md`](docs/hardware/BOM.md) for which of those are
hardware-confirmed versus best-effort placeholders.

## Flashing

Use `nrfutil device program` — confirmed working on real hardware:

```sh
nrfutil install device   # one-time setup
nrfutil device program --firmware build/Blink.hex
```

Drag-and-drop onto the DK's `JLINK` USB mass-storage drive is **not**
reliable (fails on at least the DK unit used during bring-up, see
[`docs/hardware/BOM.md`](docs/hardware/BOM.md)). The Arduino IDE's
**Upload** button calls `nrfutil` under the hood, so this only matters
if you're flashing manually.

## Verified on real hardware

Everything in this section was confirmed by halting the CPU over SWD,
reading real registers/RAM, or measuring real serial I/O — not assumed
from a clean compile:

- **`Blink`** — LED genuinely toggles, confirmed via direct GPIO
  register reads, not just visual inspection.
- **`delay(ms)`** — sleeps via System ON idle (`WFI` + a GRTC compare
  channel) instead of busy-waiting; halted the CPU 5 times across two
  sketches and found it parked at the instruction immediately after
  `wfi` every single time.
- **`Serial`** — TX and RX both confirmed bidirectionally over the DK's
  VCOM port (`SerialEcho`'s greeting received, and a sent string echoed
  back correctly). Getting here required finding and fixing four
  separate, compounding bugs — see
  [`docs/VERIFICATION.md`](docs/VERIFICATION.md)'s "Serial mystery:
  RESOLVED" section for the full account.
- **`PWMFade`** — duty-cycle RAM state sampled twice, 3 seconds apart,
  confirmed genuinely changing over real time.
- **IRQ vector wiring** — this chip's real interrupt vectors weren't
  actually wired to this core's handlers at all (a missing nrfx
  integration include, plus missing per-instance trampoline functions
  nrfx expects the integrator to provide). This silently broke
  `attachInterrupt()` entirely and `Serial`'s RX path — invisible to
  every build/link check until real hardware exposed it via a frozen
  program counter. Found and fixed project-wide.
- **BLE controller bring-up** — `mpsl_init()` → `sdc_init()` →
  `sdc_rand_source_register()` (wired to this chip's real CRACEN
  hardware entropy source) → `sdc_enable()` confirmed genuinely
  succeeding on real hardware (`examples/BLEHardwareTest`), with this
  chip's real RADIO/GRTC/TIMER10/clock IRQ vectors routed to MPSL's
  handlers.

`Wire`/`SPI`/`analogRead`/`attachInterrupt` all run on real hardware
without crashing, but still need protocol-level confirmation (actual
I2C/SPI bus data, actual button-press-to-callback firing) — tracked
honestly in [`docs/VERIFICATION.md`](docs/VERIFICATION.md), not glossed
over.

## Roadmap

- **BLE**: not exposed to sketches yet, but the controller bring-up
  sequence is implemented and hardware-verified (see above).
  `cores/nrf54l/mpsl_glue.c` is gated behind a build flag
  (`ARDUINO_NRF54_MPSL_ENABLED`) so ordinary sketches keep building
  normally. There's still no Arduino API or HCI host, so none of this
  is reachable from a normal sketch yet. See
  [`docs/BLE_ROADMAP.md`](docs/BLE_ROADMAP.md) for status and concrete
  next steps.
- **Low-power sleep modes**: System ON idle is implemented and
  hardware-verified (see above). System OFF (deep sleep) is not
  implemented yet. See
  [`docs/LOW_POWER_ROADMAP.md`](docs/LOW_POWER_ROADMAP.md) for status
  and the System OFF scoping notes.
- **`Serial` RX ring buffer**: v1 keeps a single byte in flight,
  re-armed after each `read()` — a known, disclosed limitation where
  bytes arriving faster than the sketch calls `Serial.read()` can be
  dropped. A ring-buffer RX (matching upstream Arduino cores) is a
  planned v2 item.
- **`Wire`/`SPI`/`analogRead`/`attachInterrupt`**: all run on real
  hardware without crashing; protocol-level correctness still needs
  further hardware confirmation — see
  [`docs/VERIFICATION.md`](docs/VERIFICATION.md).

Issues and PRs welcome — see [`docs/GUIDE.md`](docs/GUIDE.md) for the
architecture overview before diving in.

## Docs

- [`docs/GUIDE.md`](docs/GUIDE.md) — architecture overview and API
  reference; start here.
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — design rationale: why
  build on `nrfx`, the secure-only/no-TrustZone-M v1 scoping decision, the
  wrapper-include technique that makes vendored `extern/nrfx` sources
  visible to Arduino IDE's core auto-discovery, and what's explicitly out
  of scope for v1.
- [`docs/VERIFICATION.md`](docs/VERIFICATION.md) — what's actually been
  compiled/linked and verified versus what's still unconfirmed on real
  silicon.
- [`docs/BLE_ROADMAP.md`](docs/BLE_ROADMAP.md) — BLE bring-up status and
  next steps toward a usable Arduino BLE API.
- [`docs/LOW_POWER_ROADMAP.md`](docs/LOW_POWER_ROADMAP.md) — sleep-mode
  scoping notes and System OFF design.
- [`docs/hardware/BOM.md`](docs/hardware/BOM.md) — parts list and the
  pin-mapping caveat (best-effort placeholders pending hardware bring-up).

## License

MIT for the code in this repository (see [`LICENSE`](LICENSE)). The
`extern/nrfx` and `extern/CMSIS_6` submodules carry their own licenses
(BSD-3-Clause and Apache-2.0 respectively).
