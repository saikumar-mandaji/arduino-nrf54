# Architecture

## What this is

An Arduino-API core for the Nordic **nRF54L15** (Cortex-M33), targeting the
**nRF54L15-DK** in v1. Not an official Arduino or Nordic project -- a
from-scratch core built directly on Nordic's `nrfx` HAL/driver layer and
ARM's CMSIS-Core, since no Arduino core exists yet for the nRF54 series
(only Nordic's Zephyr-based nRF Connect SDK does).

## Why build on nrfx instead of from-scratch register access

`nrfx` (vendored as a git submodule at `extern/nrfx`) is Nordic's official,
BSD-3-Clause, standalone peripheral driver layer -- the same code used
inside nRF Connect SDK/Zephyr, just usable outside them too. Building on it
means register-level correctness (timing, EasyDMA setup, errata
workarounds) comes from Nordic's own maintained code, not reimplemented
and re-debugged from the reference manual. `CMSIS_6` (vendored the same
way at `extern/CMSIS_6`) supplies the Cortex-M33 core headers (NVIC, SysTick,
etc.) nrfx itself depends on.

Both are real git submodules, not vendored/copied source -- `git submodule
update --init` after cloning is required (see README's Quick Start).

## Secure-only image, no TrustZone-M partitioning (a v1 scoping decision)

The nRF54L15 is a TrustZone-M part: production firmware often splits into
a Secure image (bootloader, crypto, peripheral gatekeeping) and a
Non-Secure application image. This core deliberately runs as a **single
Secure-only image** -- no `NRF_TRUSTZONE_NONSECURE` define, no SAU/IDAU
configuration, no Secure/Non-Secure boundary at all.

This is a genuine simplification, not an oversight: standing up a
Secure/Non-Secure split correctly (SAU regions, non-secure callable
gateways, peripheral security attribution via `NRF_SPU`) is substantial
additional scope that doesn't change whether GPIO/Serial/timing work, and
would roughly double what has to be verified before this core is even
useful for a Blink sketch. It's an intentional line to revisit once v1 is
proven on real hardware -- documented here rather than silently assumed.

## Directory layout

```
cores/nrf54l/         -- the Arduino API implementation
  Arduino.h            top-level API header
  Print.h / .cpp        print()/println() base class
  HardwareSerial.h/.cpp  Serial, over UARTE30 (see below -- was UARTE20)
  SPI.h/.cpp               SPI, over SPIM21 (blocking transfers only)
  Wire.h/.cpp              I2C (TwoWire), over TWIM22 (blocking, master only)
  wiring_digital.c       pinMode/digitalWrite/digitalRead, over nrf_gpio
  wiring_time.c           millis/micros/delay, over GRTC
  wiring_analog.c          analogRead (SAADC) / analogWrite (PWM20)
  wiring_interrupts.c      attachInterrupt/detachInterrupt (GPIOTE)
  main.cpp                calls setup()/loop()
  syscalls.c              newlib syscall stubs (bare-metal boilerplate)
  nrfx_config.h            nrfx integration: which drivers are enabled
  nrfx_config_nrf54l15_application.h   vendored-in Nordic template, only
                           NRFX_UARTE_ENABLED/NRFX_GRTC_ENABLED/
                           NRFX_SPIM_ENABLED/NRFX_TWIM_ENABLED flipped on
  nrfx_glue.h / .c         nrfx integration: IRQ/critical-section/atomics glue
  nrfx_log.h               nrfx integration: logging (no-op, disabled)
  _nrfx_uarte.c, _nrfx_grtc.c, _nrfx_spim.c, _nrfx_twim.c,
  _nrfx_saadc.c, _nrfx_pwm.c, _nrfx_gpiote.c,
  _nrfx_flag32_allocator.c, _system_nrf54l.c,
  _startup_nrf54l15_application.S
                           see "The underscore-prefixed wrapper files" below

variants/nrf54l15dk/   -- board-specific pin mapping (pins_arduino.h)
extern/nrfx/           -- git submodule: Nordic's HAL/driver layer
extern/CMSIS_6/        -- git submodule: ARM's Cortex-M core headers
examples/Blink/, examples/SerialEcho/  -- v1 acceptance-test sketches
boards.txt, platform.txt, programmers.txt  -- Arduino IDE board package
Makefile                -- plain build, used by CI and for local dev
```

## The underscore-prefixed wrapper files

Arduino IDE / `arduino-cli`'s core-compilation step only auto-discovers
`.c`/`.cpp`/`.S` files that live **physically inside** `cores/nrf54l/`. The
actual UARTE driver, GRTC driver, flag32 allocator, `system_nrf54l.c`, and
the per-chip startup assembly all live in the vendored `extern/nrfx`
submodule, outside that directory -- so without help, `arduino-cli` silently
builds a core missing all of them (confirmed directly: the first
`arduino-cli compile` attempt during development linked successfully but
against the *wrong* startup code entirely, because none of the real nrfx
driver `.o`s or the real vector table ever made it into `core.a`).

The fix is the five `_*.c`/`_*.S` files in `cores/nrf54l/`: each one is a
single `#include "../../extern/nrfx/.../real_file.c"` line -- a textual
include of the vendored source, not a copy of it. This puts a compilation
unit inside the directory `arduino-cli` scans, while the actual code still
lives in, and is still updated via, the submodule. The plain `Makefile`
build does not need these wrappers -- it lists the real `extern/nrfx` paths
directly.

## IRQ vector wiring: reusing Nordic's own vector table, not hand-writing one

This core's startup file (`cores/nrf54l/_startup_nrf54l15_application.S`)
`#include`s Nordic's own vendored MDK startup assembly
(`extern/nrfx/bsp/stable/mdk/nrf54l/nrf54l15/gcc_startup_nrf54l15_application.S`)
verbatim, rather than hand-writing a vector table. Routing a peripheral
driver's generic ISR (e.g. `nrfx_grtc_irq_handler()`) to the specific
numbered vector Nordic's table expects (e.g. `GRTC_2_IRQHandler`) is
done entirely via nrfx's own `#define`-chain in
`extern/nrfx/bsp/stable/soc/irqs/nrfx_irqs_nrf54l15_application.h` /
`nrfx_mdk_fixups.h` -- e.g. `#define nrfx_grtc_irq_handler
GRTC_IRQHandler` and `#define GRTC_IRQHandler GRTC_2_IRQHandler`,
resolved entirely by the preprocessor before the driver source is even
compiled. No project code hand-maintains a vector table or per-peripheral
IRQ handler name.

This was compared (2026-07-21) against `lolren/nrf54-arduino-core`'s
approach, which hand-writes its own ~500-line `startup_nrf54l15.S` with
the entire nRF54L15 vector table transcribed by hand, and hardcodes a
specific peripheral-mode name at each shared serial-fabric vector slot
(e.g. the vector table entry at the SERIAL21 fabric instance is named
`SPIM21_IRQHandler`, not a mode-agnostic name). This appears to have
introduced a real bug: their file also separately defines a
`TWIM21_IRQHandler` weak stub (for their I2C/Wire driver's ISR
presumably to override), but the vector table itself never references
that name at all -- only `SPIM21_IRQHandler`. If their I2C driver's real
ISR is named to match that stub, it would silently never be called by
hardware, since the vector table points at a different symbol name for
that slot. (Not independently confirmed against their actual TWIM
driver source -- flagged as a likely bug from the vector-table/stub
mismatch alone, not verified end-to-end on their hardware.)

nrfx's own official mapping avoids this class of bug by construction:
each shared serial-fabric instance's vector name is generic
(`SERIAL21_IRQHandler`, not `SPIM21_IRQHandler`), and *every* possible
mode driver for that instance (`nrfx_spim_21_irq_handler`,
`nrfx_twim_21_irq_handler`, `nrfx_uarte_21_irq_handler`, etc. --
confirmed by reading `nrfx_irqs_nrf54l15_application.h` directly)
`#define`s down to that same one name, so whichever driver is actually
compiled in for that instance is automatically the one wired to the
vector table -- there's no separate hand-maintained name to fall out of
sync. Combined with reusing Nordic's own vetted 270-entry vector table
instead of a hand transcription, this is judged the more robust
approach and was kept as-is; no code change resulted from this
comparison.

## Timing: GRTC

`millis()`/`micros()`/`delay()` are built on the nRF54 series' GRTC
(Global Real-Time Counter), which replaces the nRF52's RTC peripheral for
this purpose. `NRF_GRTC_SYSCOUNTER_MAIN_FREQUENCY_HZ` (from
`extern/nrfx/hal/nrf_grtc.h`) is 1 MHz, so `micros()` is a direct,
unscaled read of `nrfx_grtc_syscounter_get()` -- no fixed-point math
needed. See `docs/VERIFICATION.md` for what's confirmed about this at
compile time versus what's unverified at runtime (no hardware).

## One shared "flexible serial peripheral" block per index -- why Serial, SPI, and Wire use different instance numbers

The nRF54L15 doesn't have independent UART/SPI/I2C peripherals -- each
numbered index (20, 21, 22, 30, ...) is one shared "flexible serial
peripheral" hardware block that can be configured as UARTE, SPIM, SPIS,
TWIM, or TWIS, but only as one of those roles at a time. This is directly
visible in the vendored device header: `UARTE20_IRQn`, `SPIM20_IRQn`,
`TWIM20_IRQn` etc. all `#define` to the same `SERIAL20_IRQn` (see
`extern/nrfx/bsp/stable/mdk/nrf54l/nrf54l15/nrf54l15_application.h`).

`SPI` uses index 21 (`NRF_SPIM21`) and `Wire` uses index 22
(`NRF_TWIM22`) -- confirmed independent blocks, not a guess. `Serial`
originally used index 20 (`NRF_UARTE20`) on the same "any free index
works" reasoning -- but that turned out to be wrong for a reason this
reasoning alone couldn't reveal: which index is actually **wired to the
DK's onboard USB VCOM bridge** is a hardware/schematic fact, not a free
choice. See the next section.

## Serial: UARTE30 (not UARTE20), blocking TX, single-byte polled RX

`HardwareSerial` wraps **UARTE30**, pins **P0.00 (TX) / P0.01 (RX)** --
confirmed to be the instance actually wired to the nRF54L15-DK's onboard
J-Link VCOM bridge (cross-checked against Zephyr's own
`nrf54l15dk_nrf54l15` devicetree, which routes its `uart30` node to
those exact pins for console/logging). The original choice of UARTE20
was a reasonable-sounding but wrong guess -- it produced zero serial
output on real hardware, since UARTE20 was never connected to anything
outside the chip on this board. See `docs/VERIFICATION.md`'s "Real
hardware bring-up" section for the full story, including that `Serial`
still isn't confirmed working even with the corrected instance/pins (a
separate, still-open issue, likely related to the DK's interface-MCU
configuration rather than this core's firmware).

`write()` uses `nrfx_uarte_tx(..., NRFX_UARTE_TX_BLOCKING)`.
`read()`/`available()` poll a single re-armed one-byte RX buffer -- there
is no ring-buffer/interrupt-driven RX queue yet (a known v2 item, not
implemented here; see VERIFICATION.md for the failure mode this implies:
bytes arriving faster than the sketch calls `Serial.read()` can be
dropped).

## SPI and Wire (v2)

`SPI` wraps `nrfx_spim` (instance 21), blocking transfers only --
`beginTransaction()`/`endTransaction()` apply `SPISettings` via
`nrfx_spim_reconfigure()` but don't implement any locking/queuing (v1's
single-caller assumption carries over). `Wire` wraps `nrfx_twim` (instance
22), master mode only, with small fixed 32-byte TX/RX buffers (see
`WIRE_BUFFER_LENGTH` in `Wire.h`) rather than a dynamically-sized buffer.

**CS/SS is never touched by `SPI.cpp`** -- `NRF_SPIM_PIN_NOT_CONNECTED`
is passed for nrfx's `ss_pin` config field, deliberately. Found on real
hardware: `nrfx_spim` auto-asserts/deasserts a *real* configured `ss_pin`
around every individual `nrfx_spim_xfer()` call, which breaks any
transaction built from more than one `SPI.transfer()` call where CS is
meant to stay low across all of them (e.g. a flash chip's JEDEC ID read:
command byte + 3 response bytes must happen under one continuous
CS-low period). Matches standard Arduino SPI convention on other cores
anyway -- CS is always sketch-managed via `digitalWrite()`, never
automatic. See `docs/VERIFICATION.md`'s "SPI/I2C bring-up" section for
the full bring-up story this came from.

`SPI.transfer(void *buf, size_t count)` (the in-place, single-buffer
overload) passes the same pointer as both the EasyDMA source and
destination for a `nrfx_spim_xfer` -- this matches the standard Arduino
`SPI.transfer(buf, count)` contract, but whether SPIM's DMA engine
actually handles reading and writing the same memory region safely
mid-block-transfer has not been confirmed (see docs/VERIFICATION.md).

## analogRead (SAADC) and analogWrite (PWM20) (v3)

The nRF54L15's SAADC selects its analog input by raw GPIO pin number
under the hood (`NRF_SAADC_HAS_AIN_AS_PIN`, confirmed in
`extern/nrfx/hal/nrf_saadc.h`), not via the older fixed
`NRF_SAADC_INPUT_AIN0`-style enum other nRF chips use -- nrfx exposes
this through a newer, chip-agnostic `NRFX_ANALOG_EXTERNAL_AIN0..AIN13`
enum (`extern/nrfx/helpers/nrfx_analog_common.h`) instead.
`analogRead(pin)` takes an SAADC **channel index** (0-7, i.e. `A0`..`A7`),
single-ended, 12-bit, blocking (`NULL` event handler on
`nrfx_saadc_simple_mode_set()`, same blocking-via-NULL-handler pattern as
UARTE/SPIM/TWIM).

`analogWrite(pin, value)` is built on PWM20 (a genuinely separate,
dedicated peripheral from the shared UARTE/SPIM/TWIM blocks -- confirmed
via its own `PWM20_IRQn`/`PWM21_IRQn`/`PWM22_IRQn`, distinct from
`SERIALn_IRQn`), configured in `NRF_PWM_LOAD_INDIVIDUAL` mode so its 4
output channels (`PIN_PWM0`..`PIN_PWM3`) each get an independent duty
cycle from one continuously-looping sequence
(`NRFX_PWM_FLAG_LOOP`). `analogWrite()` only works on those 4 fixed
pins -- a real hardware limitation (PWM20 has exactly 4 output channels),
not an oversight.

## attachInterrupt/detachInterrupt (v4)

Built on GPIOTE20 (`NRF_GPIOTE20`, confirmed genuinely independent
hardware from PWM20 and the shared UARTE/SPIM/TWIM blocks -- its own
peripheral entirely, not aliased to any other index). Up to 8
simultaneous interrupt pins are supported
(`NRFX_GPIOTE_CONFIG_NUM_OF_EVT_HANDLERS`, raised from nrfx's default of
2 -- see `nrfx_config_nrf54l15_application.h`).

nrfx's GPIOTE driver registers one handler *per pin registration*, not
one global handler with a pin argument passed at dispatch time the way
some other nrfx drivers work -- but the dispatch function it calls
*does* receive the pin number, so `attachInterrupt()` uses a single
shared dispatch function (`gpiote_dispatch`) for every pin and looks the
firing pin up in a small fixed table to find the sketch's actual
callback, rather than allocating a distinct C function per attached pin
(which isn't possible in C without codegen tricks anyway).

Only edge triggers (`RISING`/`FALLING`/`CHANGE`) are supported, not
level triggers (`LOW`/`HIGH`) -- this isn't an arbitrary limitation:
nrfx's own documentation for `nrfx_gpiote_input_configure()` states that
setting a level trigger while also using a GPIOTE channel for the same
pin is an invalid configuration and returns an error. `attachInterrupt()`
always requests a channel (needed for edge triggering), so level
triggers are out of reach through this code path.

## What's explicitly out of scope

- I2C/SPI slave modes, I2C multi-master arbitration
- Level-triggered (LOW/HIGH) pin interrupts (see above -- a real
  nrfx/hardware constraint, not an omission)
- Non-Secure/TrustZone-M partitioning
- Low-power sleep modes
- OTA/DFU
- The nRF54L15's radio (BLE, 802.15.4) entirely

These were excluded deliberately to get a real, buildable, honestly-scoped
core rather than a wide, shallow, mostly-untested surface.
