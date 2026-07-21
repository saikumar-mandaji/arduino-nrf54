# Verification status

Honest accounting of what has actually been checked versus what is
untested. Originally written from a development environment with no
physical nRF54L15-DK; a real DK has since been connected and used for an
initial hardware bring-up pass (see "Real hardware bring-up" below) --
most of the disclosures below now reflect that updated state, not the
original no-hardware constraint.

## Third hardware pass (2026-07-21): the IRQ vector wiring was broken project-wide

The user connected a real nRF54L15-DK and asked for a full validation
pass with no user interaction required. This found and fixed **the two
most severe bugs in this project's history** -- both completely
invisible to every compile/link check performed throughout this
project, including this session's own earlier `arm-none-eabi-nm -u`
"zero undefined symbols" checks, because a weak symbol silently
aliased to `Default_Handler` is not an *undefined* symbol.

**Bug 1: `nrfx_irqs_nrf54l15_application.h` was never included.**
`extern/nrfx/bsp/stable/templates/nrfx_templates_config.h` (nrfx's own
template) has `//#include <soc/nrfx_irqs.h>` -- commented out, left for
the integrator to add. This project's `cores/nrf54l/nrfx_config.h`
never did. Without it, every nrfx driver's generic
`nrfx_<periph>_irq_handler` function name is never renamed (via the
per-chip macro chain) to the real chip vector name -- the driver
function just compiles under its own name, unreferenced, while the
real vector table slot stays wired to `Default_Handler` (an infinite
`b .` loop).

Found by: flashing `Blink` (with this session's new System ON idle
`delay()` change, the first code in this whole project to actually
depend on the GRTC interrupt firing -- the old busy-wait `delay()`
never needed it, so this bug had no way to surface before now), halting
the CPU repeatedly over SWD, and finding **PC frozen at the exact same
address after every `go`/resume** -- `arm-none-eabi-nm` confirmed that
address was `Default_Handler`, and confirmed `GRTC_2_IRQHandler` was
still weak/aliased to it while the real `nrfx_grtc_irq_handler` function
body sat compiled-but-unreferenced at a different address entirely.
Fixed by adding `#include <soc/nrfx_irqs.h>` to `nrfx_config.h`.

**Bug 2: five peripheral drivers need integrator-provided IRQ
trampolines that never existed.** Unlike GRTC/SAADC (fixed, no-argument
handlers), `nrfx_uarte`/`nrfx_spim`/`nrfx_twim`/`nrfx_pwm`/`nrfx_gpiote`
all use a **generic, instance-parameterized** handler
(`nrfx_uarte_irq_handler(nrfx_uarte_t*)`, etc.). With
`NRFX_PRS_ENABLED=0` (this core's config, confirmed by reading it), nrfx
never wires these to the real vector table automatically -- the
integrator must write a small named function per instance (e.g. `void
nrfx_uarte_30_irq_handler(void) { nrfx_uarte_irq_handler(&s_uarte); }`)
for the same per-chip macro chain to have anything to rename. This
project never had these. Fixed by adding one to each of
`HardwareSerial.cpp`, `SPI.cpp`, `Wire.cpp`, `wiring_analog.c`, and
`wiring_interrupts.c`.

Checked (not assumed) whether this actually mattered for each affected
peripheral, by reading the relevant nrfx driver source directly:
- **GPIOTE (`attachInterrupt`)**: completely non-functional before this
  fix, on any pin, with no fallback -- interrupt-driven by definition.
  The single most severe instance of this bug.
- **UARTE (`Serial`)**: RX (interrupt-driven, `NRFX_UARTE_EVT_RX_DONE`)
  was non-functional before this fix. TX uses blocking mode, which
  nrfx implements as direct register polling -- unaffected by this
  specific bug, though it may still be affected by the older,
  separately-documented Serial mystery below.
- **SPIM (`SPI`)/TWIM (`Wire`)**: confirmed, by reading
  `nrfx_spim.c`/`nrfx_twim.c` directly, that this core's blocking-mode
  usage (`NULL` event handler) polls the completion event directly and
  does **not** depend on the interrupt vector -- so `SPI`/`Wire` were
  not actually broken by this bug, but the trampolines are still
  correct, necessary plumbing for any future non-blocking use.
- **PWM (`analogWrite`)**: same `NULL`-handler pattern as SPIM/TWIM, but
  *not* independently re-verified against `nrfx_pwm.c`'s internals --
  unconfirmed either way whether `analogWrite()` needed this vector.

**Fixed and reverified end-to-end on the real DK:**
- `Blink`: `GRTC_2_IRQHandler` is now a real, distinct symbol
  (confirmed via `nm`); the LED's GPIO OUT register (`0x50050400`,
  bit 9) was read 6 times over ~3 seconds and genuinely alternated
  between `0x000` and `0x200` -- real, confirmed blinking, not just "no
  crash."
- **System ON idle `delay()` -- the exact hardware verification
  `docs/LOW_POWER_ROADMAP.md` had flagged as still needed -- is now
  done.** Disassembled `delay()` and confirmed address `0x9ce` is the
  `wfi` instruction; repeated random halts (5 in a row, across two
  different sketches -- `Blink` and `I2CScanner`) landed **every single
  time** at `0x9d0`/`0x7b0`, the instruction immediately after `wfi`.
  This is real, direct confirmation the CPU is genuinely sleeping, not
  spinning -- a random halt would not land on the same instruction
  every time if the CPU were busy-looping through a multi-instruction
  polling path instead.
- `PWMFade`: PWM20's `ENABLE` register reads `1`; the live duty-cycle
  buffer (`s_pwm_duty`) was sampled 4 times over ~3 seconds and read
  `705 -> 301 -> 694 -> 317` (all within the valid 0-1000 range) --
  genuinely changing, confirming a real fade in progress.
- `I2CScanner`/`SPILoopback`/`AnalogReadSerial`: all three now run to
  completion repeatedly without crashing or hanging (confirmed via PC
  sampling landing inside `delay()`, post-`wfi`, consistent with a
  normal loop cycling through `Wire`/`SPI`/`analogRead()` calls without
  faulting) -- but their *protocol-level* correctness (actual I2C ACKs,
  SPI loopback byte match, ADC reading) could not be independently
  confirmed this pass: their results are only observable via `Serial`
  output, which remains the separately-documented unresolved discrepancy
  below, and `SPILoopback` additionally requires a physical MOSI-MISO
  jumper not confirmed present on this unit.
- `ButtonInterrupt`: `attachInterrupt()`'s bookkeeping is confirmed
  fully correct via live memory read (`s_gpiote_began=true`, slot 0
  shows `in_use=true`, `pin=45` (`P1.13`, matches `NRF_GPIO_PIN_MAP(1,13)`
  exactly), `channel=7`, a valid callback pointer), and GPIOTE
  channel 7's own `CONFIG` register confirms Event mode + falling-edge
  polarity with the interrupt enabled in `INTENSET1` (matching the
  `GPIOTE20_1_IRQHandler` line this core's secure/non-TrustZone-split
  build uses). **Attempted to simulate a physical press** by
  temporarily reconfiguring `P1.13` as an output, driving it low, then
  releasing it back to input (a real electrical falling-then-rising
  transition, confirmed via the `IN` register) -- `EVENTS_IN[7]` never
  latched and `ledState` never toggled. Inconclusive, not a confirmed
  failure: the `IN` register read back high the entire time the pin was
  forced low (`DIR`=output, `OUT`=0), which doesn't match a simple
  push-pull override and suggests either board-specific loading on this
  button net or an input-buffer/`PIN_CNF` interaction with `DIR` this
  session didn't fully resolve. The pin was restored to its original
  input+pull-up configuration afterward. **Still needs an actual
  physical button press to confirm end-to-end**, but the entire
  software/config chain up to the physical pin is now confirmed correct
  where it wasn't confirmable at all before this session's fix.
- **BLE controller bring-up** (`docs/BLE_ROADMAP.md`): a new
  `examples/BLEHardwareTest` sketch calling `nrf54_mpsl_ble_init()`
  was flashed with `-DARDUINO_NRF54_MPSL_ENABLED` and linked against
  the real vendored SDC/MPSL archives. Result, read back over SWD:
  `bleInitResult == 1` (success), and `LED_BUILTIN` lit solidly (not
  blinking, matching the sketch's success-path behavior) -- confirmed
  by reading the GPIO OUT register 3 times. **This is real: the entire
  `mpsl_init()` -> RADIO/GRTC/TIMER10 IRQ setup -> `nrfx_cracen_init()`
  -> `sdc_init()` -> `sdc_rand_source_register()` -> `sdc_cfg_set()` ->
  `malloc()` -> `sdc_enable()` sequence genuinely succeeds on real
  nRF54L15 silicon**, not just compiles/links. Still not a working BLE
  stack -- no HCI host, no Arduino API (see `docs/BLE_ROADMAP.md`).

**Methodology note**: before trusting the PC-freezing observation above
as proof of a hang (rather than an artifact of the test tooling), this
session explicitly verified that separate `nrfutil device` command
invocations do *not* reset the target or lose RAM state -- wrote a
distinct value to a known address, then read it back via a completely
separate command invocation, and it matched exactly.

## Real hardware bring-up (first pass)

A real nRF54L15-DK (serial 1057780839, board version PCA10156) was
connected and used to flash and debug firmware directly, via `nrfutil`
(Nordic's official CLI, installed for this) talking to the DK's onboard
J-Link over SWD. This is genuine hardware verification, not just
compile-time checking -- including using the debug probe to halt the CPU
mid-execution and read live registers/memory to diagnose failures, not
just "flash and hope."

**Confirmed working on real silicon:**
- The firmware boots and runs correctly -- confirmed by halting the CPU
  over SWD and reading the program counter, which landed inside real
  application code (`loop()` → `HardwareSerial::available()`), not stuck
  at the reset vector or in a fault handler.
- `digitalWrite()`/GPIO output genuinely works: `Blink` was flashed,
  and after the fixes below, **the DK's LED1 visibly blinks** (confirmed
  by the user looking at the board), at the expected ~1 Hz rate --
  meaning `pinMode()`, `digitalWrite()`, and the GRTC-based `delay()`
  are all correct end-to-end on real hardware.
- Drag-and-drop flashing (copying a `.hex` to the `JLINK` USB
  mass-storage drive) does **not** work on this DK/interface-firmware
  combination -- it produces a `FAIL.TXT` on the drive stating "The
  currently active SWD interface does not support MSD drag and drop."
  `nrfutil device program` (SWD-based) works reliably instead;
  `docs/hardware/BOM.md`'s drag-and-drop instructions should be treated
  as a fallback to try, not the primary method.

**Two real bugs found and fixed via this hardware bring-up, that no
amount of compile-time checking could have caught:**

1. **`Serial` was on the wrong UARTE instance.** `HardwareSerial.cpp`
   used `NRF_UARTE20`, following the "just pick a free shared-block
   index" reasoning in `docs/ARCHITECTURE.md`. On real hardware this
   produced zero serial output in either direction. Research into the
   DK's actual schematic/devicetree (cross-checked against Zephyr's own
   `nrf54l15dk_nrf54l15` board files) revealed the DK's onboard J-Link
   VCOM bridge is wired to **UARTE30**, pins **P0.00 (TX) / P0.01
   (RX)** specifically -- not an arbitrary free instance. Fixed by
   switching to `NRF_UARTE30` and the correct pins. **Status update:**
   after this fix, the user reported seeing live output on the DK's VCOM
   port using Nordic's own nRF Connect for Desktop "Serial Terminal"
   app. Independent automated verification attempted separately (see
   "Genuine unresolved discrepancy: Serial" below) could not reproduce
   this via two different serial libraries -- so this is not a clean
   confirmed-pass, but the balance of evidence (a known-good vendor tool
   reportedly worked) suggests the UARTE30/pin fix itself is correct.
   **Superseded (2026-07-21): this was still wrong.** Re-reading
   Zephyr's real devicetree directly showed `zephyr,console = &uart20`
   -- UARTE30 is a real, separately-configured UART on this board, just
   not the one bridged to the automated-tooling-visible VCOM port.
   Reverted to `NRF_UARTE20`, this time with the correct pins
   (P1.04/P1.05, not the earlier guessed P1.00/P1.01). See "Serial
   mystery: RESOLVED" below for the full, final account.
2. **`nrfx_grtc_syscounter_start(true, NULL)` crashed with a HardFault.**
   After fixing UART, `Blink` was flashed and the LED stayed solidly on
   instead of blinking. Halting the CPU over SWD showed it wasn't even
   crashed at that point -- it was legitimately spinning inside
   `nrfx_grtc_syscounter_get()`, called from `delay()`'s busy-wait,
   because the syscounter never actually started (the previously-assumed
   `NRFX_GRTC_CONFIG_AUTOEN`/`AUTOSTART` config options did not bring it
   up on real silicon the way nrfx's documentation implied). The fix
   attempt -- explicitly calling `nrfx_grtc_syscounter_start(true, NULL)`
   -- immediately crashed instead: halting the CPU again showed
   `HardFault_Handler`, and reading the Cortex-M fault registers
   (`CFSR`=`0x8200`, `HFSR`=`0x40000000`, `BFAR`=`0x00000000`) identified
   a precise BusFault at address 0, escalated to HardFault. Reading the
   exception stack frame off the stack pointer pinpointed the exact
   faulting instruction (`strb r4, [r5, #0]` inside nrfx's internal
   `channel_allocate()`, called from `nrfx_grtc_syscounter_start()`):
   the driver writes through its `p_main_cc_channel` output parameter
   **unconditionally, with no NULL check**, despite the header comment
   reading like an optional output. Fixed by passing a real `static
   uint8_t` instead of `NULL`. After this fix, `Blink`'s LED visibly
   blinks correctly.

**Pin corrections confirmed against Zephyr's own
`nrf54l15dk_nrf54l15` devicetree** (not placeholders anymore for these
two):
- `LED_BUILTIN`: was `P1.09`/active-low (wrong on both counts) → now
  `P2.09`/**active-high**, matching `led0` in the real devicetree.
  Visually confirmed blinking on real hardware.
- `BTN1`: was `P1.02` (wrong -- that pin isn't even a button on the real
  board) → now `P1.13`, matching `button0`/`sw0` in the real devicetree.
  Not yet physically pressed/confirmed (see below).

## SPI/I2C bring-up against real onboard chips (second pass)

The DK has two real onboard I2C-capable/SPI-capable chips that don't
require any external wiring to test against: an **MX25R6435F** 64Mbit
SPI-NOR flash chip (SPI00: SCK=P2.01, MOSI=P2.02, MISO=P2.04, CS=P2.05,
RESET=P2.00, JEDEC ID `C2 28 17`) and an **nPM1300** PMIC with an I2C
interface (commonly on `i2c22`, address `0x6B`), both confirmed to exist
on this board via web research (Zephyr devicetree / Nordic
documentation) -- probed via a since-removed internal diagnostic sketch
(`HWSelfTest`, not part of the public example set; its findings are
fully captured here) written specifically to
probe both without needing a logic analyzer, jumper wires, or Serial to
work: it stores results in fixed global variables read back directly
over SWD.

`PIN_SPI_SCK`/`MOSI`/`MISO`/`SS` in `variants/nrf54l15dk/pins_arduino.h`
were updated from arbitrary placeholder pins to the flash chip's real
SPI00 wiring above (SPI is built on SPIM21, a different shared-block
index than SPI00, but nRF54 peripheral pins are fully remappable so
SPIM21 can drive the same physical pins the schematic wires to SPI00).

**Two real bugs found and fixed in the `SPI` implementation itself**
(not just pin guesses -- these affect every future SPI user of this
core, found while debugging why the flash chip's JEDEC ID read back as
all zeros):

1. **`nrfx_spim` auto-toggles CS around every single transfer when given
   a real `ss_pin`.** `SPI.cpp`'s `begin()`/`beginTransaction()` passed
   `PIN_SPI_SS` straight into `NRFX_SPIM_DEFAULT_CONFIG`'s `ss_pin`
   field. Reading a flash chip's JEDEC ID needs one continuous CS-low
   period across a command byte plus 3 response bytes -- but with a real
   `ss_pin` configured, nrfx asserts/deasserts CS around *each*
   `SPI.transfer()` call independently, so every byte after the first
   was received as a fresh, invalid command (explaining the all-zero
   result). Fixed by passing `NRF_SPIM_PIN_NOT_CONNECTED` for `ss_pin`
   and leaving CS entirely to the sketch via `digitalWrite()` -- which
   is also the standard Arduino SPI convention on other cores (CS is
   never auto-managed by the SPI library itself).
2. **Missing `pinMode(PIN_SPI_SS, OUTPUT)`** in the test sketch itself
   (an example bug, not a core bug, but a real one): after fix #1, CS is
   entirely the sketch's responsibility, including configuring its
   *direction* -- without an explicit `pinMode(OUTPUT)` call,
   `digitalWrite()` on a pin still left as the default `INPUT` has no
   electrical effect, so CS silently never asserted.

**After both fixes, the JEDEC ID still read back as `00 00 00`, not the
expected `C2 28 17`.** Further diagnosis directly ruled out a
pin-mapping error: the actual configured `PSEL.SCK`/`PSEL.MOSI`/`PSEL.MISO`
registers were read back live over SWD (`0x500C7600`-`0x500C7608` on
`SPIM21`'s real Secure base address `0x500C7000`, offsets found from the
vendored `.svd` file) and matched the expected encoded pin values
(`0x41`/`0x42`/`0x44`) exactly -- confirming the peripheral genuinely is
wired to P2.01/P2.02/P2.04 as intended. A longer reset-recovery delay
(50ms instead of 1ms after releasing the flash chip's RESET line) also
made no difference. The remaining cause is unidentified without a scope
or logic analyzer on the physical bus -- possibilities include a
chip-specific wake sequence (e.g. release from deep power-down) not yet
implemented, a still-wrong assumption about this specific DK's hardware
revision, or the flash chip not being populated on this particular unit.
**Not resolved as of this pass; the pin-mapping and CS-timing bugs
above are real, fixed, and valuable independent of this outcome.**

**I2C**: the diagnostic sketch's full-bus scan (0x08-0x77, including the PMIC's
expected `0x6B`) found **zero devices** on `PIN_WIRE_SDA`/`PIN_WIRE_SCL`
(still unconfirmed placeholder pins -- unlike the SPI flash chip, the
PMIC's exact SDA/SCL pin wiring for this specific DK could not be found
in mainline Zephyr's board files during this pass, so `Wire`'s pins are
still a guess, not a confirmed value like `LED_BUILTIN`/`BTN1`/SPI).

## Serial mystery: RESOLVED (2026-07-21)

The long-standing "Serial produces no output visible to any automated
tooling, but a human using Nordic's own Serial Terminal app reported
seeing it" discrepancy is now root-caused and fixed. It turned out to
be **four separate, compounding issues**, not one:

1. **Wrong UARTE instance/pins.** This project previously used UARTE30
   (TX=P0.00/RX=P0.01), based on an earlier belief that an UARTE20
   attempt had failed -- but that earlier attempt used *guessed* pins
   (P1.00/P1.01). Re-reading Zephyr's real
   `nrf54l15dk_nrf54l_05_10_15_cpuapp_common.dtsi` directly shows the
   DK's actual chosen console is `zephyr,console = &uart20`, and
   `nrf54l15dk_nrf54l_05_10_15-pinctrl.dtsi` gives uart20's real pinctrl
   as TX=P1.04, RX=P1.05 (RTS=P1.06, CTS=P1.07, not wired up by this
   core -- 2-wire only). Fixed: `cores/nrf54l/HardwareSerial.cpp` now
   uses `NRF_UARTE20`, and `variants/nrf54l15dk/pins_arduino.h` sets
   `PIN_SERIAL_TX`/`PIN_SERIAL_RX` to P1.04/P1.05.
2. **The DK's onboard debug/VCOM UART bridge tri-states its UART pins
   until the host terminal asserts DTR** -- confirmed directly from
   Nordic's nRF54L15-DK Hardware User Guide: the bridge routes through
   analog switches (U4/U5) gated by the virtual COM port's DTR line.
   Every prior automated-tooling attempt in this project never asserted
   DTR, so it saw zero bytes even on firmware that was transmitting
   correctly. Fix: no firmware change needed, just DTR must be asserted
   host-side (`SerialPort.DtrEnable = true` in .NET, equivalent in
   Python `pyserial`). Also empirically discovered on this DK unit:
   **UARTE20 bridges to `COM21` (vcom:1), not `COM20`** (vcom:0) --
   contrary to the naive vcom-index assumption.
3. **EasyDMA requires the TX buffer to be in Data RAM.**
   `extern/nrfx/drivers/src/nrfx_uarte.c`'s `poll_out()` calls
   `nrf_dma_accessible_check()` (`extern/nrfx/hal/nrf_common.h`: `(addr
   & 0xE0000000) == 0x20000000`) and silently returns `-EINVAL` per byte
   for any buffer outside SRAM. `HardwareSerial::write(const uint8_t*,
   size_t)` used to hand the caller's pointer straight to
   `nrfx_uarte_tx()` -- fine for `Serial.write(uint8_t)` (always a
   stack variable), but broken for `Serial.print("literal")`/`println`,
   whose flash-resident string-literal pointer resolves through this
   class's own bulk-write override (shadowing `Print`'s own safe
   byte-by-byte default). Every `print`/`println` with a string literal
   sent nothing, matching "0 bytes" exactly regardless of instance/pins.
   Fixed by copying through a small RAM staging buffer before calling
   `nrfx_uarte_tx()` (see the file comment above `write()`).
4. **`nrfx_uarte_rx_buffer_set()` alone never starts reception.**
   Confirmed directly from `nrfx_uarte.h`'s doc comment for
   `nrfx_uarte_rx_enable()`: the receiver must be explicitly enabled,
   which synchronously fires `NRFX_UARTE_EVT_RX_BUF_REQUEST`, and the
   app must respond by calling `rx_buffer_set()` -- the same event
   fires again every time the current buffer fills, so the app must
   keep re-supplying it from the event handler. This project called
   `rx_buffer_set()` from `begin()`/`read()` but never called
   `rx_enable()` at all, so RX was never actually started -- matching
   the previously-observed `PSEL.RXD == 0xFFFFFFFF` (disconnected)
   register read exactly. Fixed: `begin()` now calls
   `nrfx_uarte_rx_enable(&s_uarte, 0)` once, and
   `uarte_event_handler()` now handles `NRFX_UARTE_EVT_RX_BUF_REQUEST`
   by re-supplying the single-byte buffer indefinitely.

**Hardware-confirmed working, both directions**, via a PowerShell
`System.IO.Ports.SerialPort` session on `COM21` with `DtrEnable`/
`RtsEnable` explicitly set true (both default `false` in .NET):
`SerialEcho`'s startup greeting ("arduino-nrf54 SerialEcho ready") was
received cleanly, and a sent string ("Hello!") was echoed back exactly.

**Known remaining limitation (not a bug, already scoped in v1):** a
second round-trip test with a longer string ("The quick brown fox
12345") echoed back with two spaces silently dropped
("Thequickbrown fox12345"-style loss). This matches the single-byte
RX buffer's documented limitation (see the comment above `s_rx_byte` in
`HardwareSerial.cpp`): with no ring buffer, a byte arriving before
`loop()` calls `Serial.read()` for the previous one overwrites it. A
ring-buffer RX (matching upstream Arduino cores) remains a known v2
follow-up, not a regression from this fix.

## What IS verified (compile-time / pre-hardware)

Unlike most projects in this portfolio, this one had a real ARM GCC
toolchain available during development (the ARM GNU Toolchain 14.2.Rel1,
installed specifically for this project), so the following is **actual,
real compilation and linking**, not just design review:

- **Plain `Makefile` build**: both `make EXAMPLE=Blink` and
  `make EXAMPLE=SerialEcho` compile every core source file (`nrfx_glue.c`,
  `wiring_digital.c`, `wiring_time.c`, `syscalls.c`, `Print.cpp`,
  `HardwareSerial.cpp`, `main.cpp`), the real vendored `nrfx_uarte.c`,
  `nrfx_grtc.c`, and `nrfx_flag32_allocator.c` drivers, Nordic's real
  `system_nrf54l.c` and `gcc_startup_nrf54l15_application.S`, and link
  cleanly against the real `nrf54l15_xxaa_application.ld` linker script --
  producing a valid ELF with a correctly split `.data` section (flash LMA
  vs RAM VMA), a real vector table at the flash origin, and a `.hex` output.
  Verified via `arm-none-eabi-size`: ~13.5 KB text for Blink, ~12.8 KB for
  SerialEcho, both comfortably inside the DK's 1.5 MB flash / 256 KB RAM.
- **`arduino-cli compile` against the real `boards.txt`/`platform.txt`**:
  both examples were compiled through an actual `arduino-cli` install
  (v1.5.1) with this repository set up as a real Arduino hardware package
  (via a directory junction, since `arduino-cli` requires the package to
  live under `<sketchbook>/hardware/<vendor>/<arch>/`), confirming the
  Arduino IDE integration -- not just the Makefile path -- genuinely
  builds. `arduino-cli` reported: "Sketch uses 7532 bytes (0%) of program
  storage space" for Blink, "6928 bytes (0%)" for SerialEcho.
  - This surfaced and fixed two real bugs that the Makefile path alone
    would never have caught: (1) `compiler.c.flags`/`compiler.cpp.flags`/
    `compiler.S.flags` in `platform.txt` were missing `-c`, causing the
    "compile" step to attempt a full compile+link with the wrong
    (hosted-mode) C runtime; (2) `arduino-cli`'s core auto-discovery only
    scans files physically inside `cores/nrf54l/`, so the vendored nrfx
    driver/startup files (which live under `extern/nrfx/`) were silently
    excluded from the build until the `_*.c`/`_*.S` wrapper-include files
    were added (see `docs/ARCHITECTURE.md`).
- **Register/API surface, checked against the real vendored headers**,
  not assumed from memory: `NRF54L15_XXAA` device-selection macro,
  `NRF_UARTE20` register base resolution (Secure vs Non-Secure aliasing),
  `NRF_GPIO_PIN_MAP`/pull-mode enum names, `nrfx_uarte_tx`'s blocking-mode
  flag, `nrfx_grtc_syscounter_get()`, and
  `NRF_GRTC_SYSCOUNTER_MAIN_FREQUENCY_HZ` (1 MHz) were all confirmed
  present with the expected signature/value directly in
  `extern/nrfx/**` before being used, not guessed and left for CI to
  catch.
- **CI** (`.github/workflows/ci.yml`) runs the same `Makefile` build on
  `ubuntu-latest` with `gcc-arm-none-eabi` from apt, for all four examples,
  on every push -- so this isn't a one-time local result; it's continuously
  re-checked.
- **v2 (SPI/Wire)**: `nrfx_spim`/`nrfx_twim` API surface (`NRFX_SPIM_INSTANCE`,
  `NRFX_SPIM_DEFAULT_CONFIG`, `nrfx_spim_xfer`/`NRFX_SPIM_XFER_TRX`,
  `nrfx_twim_init`/`nrfx_twim_enable`, `NRFX_TWIM_XFER_DESC_TX/RX`) was
  confirmed directly against the vendored headers before use, same as v1.
  The `UARTE20`/`SPIM21`/`TWIM22` instance-index separation was confirmed
  by checking that `UARTE20_IRQn`, `SPIM21_IRQn`, `TWIM22_IRQn` map to
  three genuinely different `SERIALn_IRQn` values (not assumed). Both new
  examples (`I2CScanner`, `SPILoopback`) build and link clean via the
  Makefile *and* `arduino-cli`. Importantly, `arduino-cli`'s
  archive-based linking confirmed that adding SPI/Wire to the core did
  **not** bloat `Blink`'s actual flashed size -- it still reports exactly
  "7532 bytes" via `arduino-cli`, unchanged from before SPI/Wire existed,
  because unreferenced `core.a` members are correctly excluded. (The
  plain `Makefile` build, by contrast, links every core object file
  directly on the command line rather than through an archive, so
  whether an unused translation unit's code actually gets discarded
  depends on linker section-level garbage collection rather than
  archive member selection -- in practice this means files with a
  forced-referenced global C++ object (like `SPI`/`Wire`'s `SPIClass
  SPI;`/`TwoWire Wire;` instances) get fully retained even when unused,
  while plain-C files with no such forced reference (like
  `wiring_analog.c`) get correctly discarded when the sketch doesn't
  call into them -- confirmed directly: `Blink`'s Makefile-built size
  grew when SPI/Wire were added in v2, but stayed exactly the same
  after SAADC/PWM were added in v3. `arduino-cli`'s number is the one
  that reflects what actually gets flashed via the Arduino IDE path.)
- **v3 (analogRead/analogWrite)**: `NRFX_ANALOG_EXTERNAL_AIN0`
  (the correct, chip-agnostic analog-input enum for a chip with
  `NRF_SAADC_HAS_AIN_AS_PIN` set, which the nRF54L15 has) and
  `nrfx_pwm_init()`'s 4-argument signature were both confirmed by fixing
  real compiler errors first -- the initial code guessed the older
  `NRF_SAADC_INPUT_AIN0`-style enum (which doesn't exist for this chip
  at all, `nrf_saadc.h`'s enum block is conditionally compiled out) and
  called `nrfx_pwm_init()` with only 3 arguments; both were caught by
  the real compiler, not left for CI to find. `AnalogReadSerial` and
  `PWMFade` build and link clean via the Makefile *and* `arduino-cli`,
  with a regression check confirming all four earlier examples still
  build too.
- **v4 (attachInterrupt/detachInterrupt)**: one real compiler-caught bug
  fixed the same way as the rest of this project -- `nrfx_gpiote_trigger_enable()`
  takes 3 arguments (`p_instance`, `pin`, `int_enable`), not 2 as first
  written. `ButtonInterrupt` builds and links clean via the Makefile and
  `arduino-cli`. A **full regression pass across all 7 examples** (the
  four from v1/v2/v3 plus `AnalogReadSerial`, `PWMFade`,
  `ButtonInterrupt`) was run through both build systems after this
  change, confirming nothing broke and no example's flashed size (via
  `arduino-cli`) drifted: Blink stayed at exactly 7532 bytes throughout
  the entire v1-v4 history of this repo.

## PWM and attachInterrupt: additional SWD-based checks (no scope/button needed)

Two more checks that don't need Serial, a scope, or physically pressing
anything:

- **`PWMFade`'s duty-cycle RAM buffer (`s_pwm_duty`) was sampled twice,
  3 seconds apart, over SWD.** `channel_0` (the only channel `PWMFade`
  drives) read `670`, then `843` on the second sample -- genuinely
  different, both within the valid `0`-`1000` duty range, consistent
  with an active fade in progress. This confirms `analogWrite()`'s
  software/data-structure side is working correctly on real hardware
  (the values really do change over real time); it does not by itself
  confirm the physical PWM waveform on `PIN_PWM0` is correct, which
  needs a scope.
- **`ButtonInterrupt`'s NVIC interrupt-enable state was read directly**
  (`NVIC->ISER[6]` at `0xE000E118`) after flashing and resetting: bit 27
  was set (`0x08000000`), corresponding to IRQn 219 (`GPIOTE20_1_IRQn`)
  -- not IRQn 218 (`GPIOTE20_0_IRQn`) as first assumed, but still a
  real, correctly-armed GPIOTE interrupt line. This confirms
  `attachInterrupt()` genuinely configured and enabled a hardware
  interrupt on real silicon; it does not confirm the ISR actually fires
  on a real button press, which still needs someone to press it.

## What is NOT verified

- ~~`Serial` still produces no observed output~~ **RESOLVED (2026-07-21)
  -- see "Serial mystery: RESOLVED" above.** Four compounding bugs (wrong
  UARTE instance/pins, DK's DTR-gated VCOM bridge, EasyDMA RAM-only TX
  buffers, missing `nrfx_uarte_rx_enable()`), all fixed; TX and RX both
  hardware-confirmed working via `SerialEcho` over `COM21`. One known,
  already-scoped-for-v2 limitation remains: the single-byte RX buffer
  can drop bytes under back-to-back load (no ring buffer yet).
- **`micros()`/`delay()` accuracy is unverified quantitatively** -- the
  fix above got `delay()` un-stuck and the LED blinks at a
  visually-roughly-correct rate, but no scope/logic-analyzer measurement
  has confirmed the actual timing accuracy (e.g. whether 500ms really
  is 500ms, or off by some clock-calibration factor).
- **`BTN1`'s corrected pin (P1.13) has not been physically pressed and
  confirmed** -- only cross-checked against the devicetree, not
  hardware-tested with `ButtonInterrupt`.
- **`PIN_SPI_*` are now confirmed** (against the onboard flash chip's
  real wiring, plus a direct SWD register read of `PSEL.SCK`/`MOSI`/`MISO`
  -- see "SPI/I2C bring-up" above), but **actual SPI data exchange with
  a real device is still not confirmed working** -- the JEDEC ID read
  came back all-zero even with correct pins and CS timing.
- **`PIN_WIRE_*`, `A0`-`A7`, `PIN_PWM0`-`PIN_PWM3` are still unconfirmed
  placeholders.** The same wrong-pin/wrong-instance risk that hit
  `Serial`, GRTC, and (partially) SPI could apply to any of these too.
  `SPILoopback`, `AnalogReadSerial`, `PWMFade`, and `ButtonInterrupt`
  have not been flashed and tested on real hardware yet (`I2CScanner`
  has, and found nothing -- see above).
- **UARTE RX robustness**: the single-byte re-armed RX path in
  `HardwareSerial.cpp` has an inherent dropped-byte risk under load (see
  `docs/ARCHITECTURE.md`) that has not been characterized against a real
  UART source, because there is no hardware to send bytes from.
- **`nrfjprog`/`nrfutil` upload recipe** in `platform.txt`
  (`tools.nrfjprog.upload.pattern`) has never been executed -- the command
  shape is standard nrfjprog usage, but has not been run against a real
  DK or even a real nrfjprog install.
- **Secure-only (no TrustZone-M split) build correctness on silicon**: the
  `.ld`/startup files are Nordic's own for the "application" core target,
  used exactly as shipped, but whether a Secure-only image boots cleanly
  on real nRF54L15 silicon (SPU default configuration, UICR, etc.) is
  unverified.
- **newlib syscall stub linkage quirk**: in the `arduino-cli` build, the
  linker pulled newlib-nano's own no-op `_close`/`_lseek`/`_read`/`_write`
  stubs (from `libc_nano.a`) rather than this core's `syscalls.c`
  versions, because nothing in the Blink/SerialEcho examples actually
  calls those functions early enough in the link for `core.a`'s objects to
  be pulled in for them. Functionally harmless for both examples (neither
  does file I/O), but worth knowing about if a future sketch relies on
  `syscalls.c`'s specific behavior over newlib's defaults.

- **SPI in-place transfer aliasing**: `SPI.transfer(buf, count)` uses the
  same buffer as both the EasyDMA source and destination pointer for
  `nrfx_spim_xfer`. Whether this is actually safe for a block transfer
  (versus the byte-at-a-time semantics the Arduino API implies) has not
  been confirmed -- no way to test it without real SPI hardware and a
  logic analyzer. `SPI.transfer(tx_buf, rx_buf, count)` (separate
  buffers) doesn't have this concern.
- **`SPILoopback` has never seen a real bus** -- it requires an actual
  MOSI-to-MISO jumper on real pins to mean anything, and only proves it
  *builds*, not that the underlying transfer works.
- **`I2CScanner` has been run on real hardware** (see "SPI/I2C bring-up"
  above) and found zero devices, on still-unconfirmed placeholder
  SDA/SCL pins -- this is not evidence the `Wire` implementation itself
  is broken (it never crashed, and reported a clean empty scan rather
  than hanging), but it's also not a positive confirmation, since it
  found nothing to confirm against.
- **Wire's fixed 32-byte buffers** (`WIRE_BUFFER_LENGTH`): `write()`
  silently drops bytes past that limit, and `requestFrom()` silently
  truncates a request larger than that limit rather than erroring loudly
  -- matches how many small embedded Wire implementations behave, but
  worth knowing about if a sketch assumes unlimited buffer size.

- **`AnalogReadSerial` has never seen a real analog signal or scope
  trace**; `PWMFade`'s duty-cycle values were confirmed genuinely
  changing over real time via a direct SWD RAM read (see "PWM and
  attachInterrupt" above), which is real (if partial) evidence -- but
  neither the actual voltage-to-reading accuracy on A0 nor the physical
  PWM waveform on `PIN_PWM0` has been confirmed with a scope.
- **PWM's `NRF_PWM_LOAD_INDIVIDUAL` sequence buffer
  (`s_pwm_duty`)** is a single static struct reused and re-submitted to
  `nrfx_pwm_simple_playback()` on every `analogWrite()` call -- untested
  whether restarting a looping sequence mid-loop (rather than only
  updating it between loop iterations) causes any visible glitch on the
  real output waveform.

- **`ButtonInterrupt` has never seen a real button press.** Confirmed on
  real hardware that `attachInterrupt()` genuinely arms a real NVIC
  interrupt line (see "PWM and attachInterrupt" above) and, as of the
  2026-07-21 IRQ vector fix (see the top of this document), that the
  vector actually reaches this core's dispatcher rather than
  `Default_Handler`. A simulated press via GPIO bit-banging over SWD
  was attempted and was inconclusive (see the same section) -- whether
  the ISR actually fires on a real physical edge, whether the
  dispatch-table lookup behaves correctly under real interrupt timing,
  and whether `pinMode(BTN1, INPUT_PULLUP)` + `attachInterrupt(BTN1,
  ..., FALLING)` is the right pull/edge combination for the DK's actual
  button wiring are all still unconfirmed without physically pressing
  it.

## Known limitations (see docs/ARCHITECTURE.md for the full list)

No I2C/SPI slave modes, no level-triggered pin interrupts, no
TrustZone-M partitioning, no low-power sleep, no OTA, no radio (BLE/
802.15.4) -- all deliberately out of scope, not overlooked.
