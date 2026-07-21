# BLE: scoping notes + vendoring status

This document records the research and initial vendoring work done so
far toward adding BLE support to this core.

**Status (2026-07-21): the real Nordic SDC + MPSL binaries are vendored
under `extern/nordic_sdc/`**, **the four application-side glue
functions MPSL needs to link are implemented**, **the GRTC/MPSL
ownership question is resolved** (no conflict, by chip design -- a real
latent channel-mask bug was found and fixed along the way), and **this
chip's real IRQ vectors are routed to MPSL's exported handlers**
(`cores/nrf54l/mpsl_glue.c`, gated behind `ARDUINO_NRF54_MPSL_ENABLED`
since this project has no opt-in BLE library yet -- see "IRQ vector
routing implemented" below). No BLE functionality is wired up yet -- no
Arduino API, no HCI bridge, and nothing calls `mpsl_init()`/
`sdc_init()`/`sdc_enable()` yet, so these vectors are inert in practice
until that happens (see "Concrete next steps"). This is groundwork, not a
working BLE stack.

## Why this isn't a quick add

Unlike the nRF52 series, the nRF54L15 has **no classic SoftDevice**
(there's no precompiled `s140`-style binary you drop in and call it a
day). Nordic's current BLE stack for nRF54-series parts is split into
two pieces, both distributed via the `sdk-nrfxlib` repo:

- **SoftDevice Controller (SDC)** -- the link-layer/controller. This is
  a **precompiled static library**, not source, built per architecture
  variant (soft-float/softfp/hard-float). It implements the Bluetooth
  Core spec's Controller role and speaks standard HCI upward.
- **MPSL (Multiprotocol Service Layer)** -- a required dependency of
  SDC that arbitrates radio time between BLE and other 2.4GHz protocols
  (802.15.4, proprietary) sharing the same radio.

SDC + MPSL together are RTOS-agnostic (confirmed: they don't require
Zephyr), which is what makes them usable from a bare Arduino core at
all. But being a precompiled binary means:

- We can vendor the `.a`/`.o` for the nRF54L15 specifically (same
  pattern as this repo already uses for pruning `nrfx`'s MDK headers to
  just the nRF54L15 variant), but we cannot build it ourselves or
  inspect/patch its internals.
- License terms for redistributing it inside this MIT-licensed open
  source core need to be checked against Nordic's actual license file
  in `sdk-nrfxlib` before shipping it (typically a Nordic 5-Clause
  license permitting binary redistribution for use on Nordic ICs --
  needs confirming against the real file, not assumed).
- The SDC docs note that **secure-mode operation is the default/mature
  path and non-secure (TrustZone-split) mode is still experimental**.
  This is actually good news for us: this core is already scoped as
  secure-only (no TrustZone-M split, see `docs/ARCHITECTURE.md`), which
  lines up with SDC's better-supported mode rather than fighting it.

## Host stack: NimBLE, but not its own controller

NimBLE (Apache-licensed, already used successfully as an Arduino BLE
host by projects like `h2zero/NimBLE-Arduino`) is the natural open host
stack to pair with SDC. But NimBLE ships its **own** controller/PHY
driver too, and that driver only supports nRF51/nRF52/nRF5340's RADIO
peripheral layout -- **not** the nRF54L15's radio hardware. So the plan
is *not* "port NimBLE wholesale"; it's:

- Use **Nordic's SDC as the controller** (it already supports the
  nRF54L15's actual radio).
- Use **NimBLE's host-only build** (it explicitly supports being paired
  with an external/vendor controller over HCI -- this is the same split
  Apache Mynewt's own `blehci` tooling uses).
- Bridge the two over a **local, in-process HCI transport** (function
  calls / shared buffers, not a real UART loop) since both pieces run
  on the same MCU.

This is the same controller/host split architecture Nordic's own nRF
Connect SDK uses internally (Zephyr's BLE host talking to SDC over
HCI); we'd be doing the same split with NimBLE as the host instead of
Zephyr's.

## Reference repos found (2026-07-21)

- **`lolren/nrf54-arduino-core`** (GitHub, MIT + third-party notices,
  created 2026-02, actively released -- confirmed real via `gh repo
  view`/`gh api`, not just its own README claims). This is an Arduino
  core for nRF54L targeting Seeed XIAO nRF54L15/LM20A + a Nordic DK
  target, and it vendors **real Nordic SDC + MPSL prebuilt binaries**
  at `hardware/nrf54l15clean/nrf54l15clean/libraries/
  Nrf54L15-Clean-Implementation/third_party/nordic_sdc/lib/nrf54l/`
  (`libsoftdevice_controller_multirole.a`, `libmpsl.a`,
  `libmpsl_fem_common.a`, soft-float ABI, built from Nordic `nrfxlib`
  `v3.4.0-rc1-12-g7a07f89ee`) -- confirmed by reading the vendored
  `VERSION`/`LICENSE` files directly via the GitHub API, not the
  README's prose. **This directly resolves open question #2 below**:
  SDC/MPSL genuinely can be vendored and linked into a non-Zephyr,
  non-NCS Arduino-style build -- someone has actually done it.
  - **Its license file also resolves open question #1**: the vendored
    `LICENSE` is `LicenseRef-Nordic-5-Clause` (Nordic Semiconductor ASA,
    2018), and clause 2 explicitly permits binary-form redistribution
    (not just "use") as long as the copyright notice is kept and the
    result is only used with a Nordic IC -- so vendoring the compiled
    `.a` inside this MIT repo's release archives is licensed, the same
    way this repo already handles `nrfx`/`CMSIS_6` license carry-through.
  - **Caveat -- treat the README's feature claims skeptically.** Its
    README claims a huge surface (full custom BLE host+link-layer stack
    with LE Secure Connections/privacy/HID/ANCS, plus experimental
    Zigbee, an OpenThread port, and Matter primitives) built from a repo
    created only ~5 months prior. That's an implausible amount of
    genuinely-tested work for that timeframe from a small account (37
    stars), and the README's own "Bare Metal / no SoftDevice" framing
    is misleading given the real vendored SDC binaries found above --
    the actual controller is Nordic's own compiled stack, not something
    written from scratch against raw radio registers. Use this repo as
    a **build-system/vendoring reference** (how they lay out the `.a`
    files, what license files they carry, what nrfxlib revision they
    pulled), not as a source of verified technical facts about what
    "works" -- re-verify anything borrowed from it on real hardware
    before trusting it, per this project's own standing practice.
  - Also worth a look before implementing: their
    `nrf54l15_hal_ble_radio_tail.inc` (how they wire nrfx/CMSIS interrupt
    vectors to the SDC binary) and their packaged `docs/
    TWO_BOARD_RELEASE_GATE.md` (what a two-board BLE regression test
    actually needs to check), as concrete examples to compare against,
    not to copy uncritically.
- **Nordic's own "Bare Metal option for nRF Connect SDK" for nRF54L
  Series** (launched officially ~August 2025, per
  nordicsemi.com/Products/Development-software/nRF-Connect-SDK/
  Bare-Metal-option-for-nRF54L-Series): ships `S115` (peripheral/
  broadcaster, up to 2 links) and a forthcoming `S145` (central/
  multirole, up to 5 links) SoftDevice-style API restoring the
  nRF5-SDK-like `sd_ble_*`/`sd_softdevice_enable()` calling convention
  on top of the same underlying RTOS-agnostic SDC/MPSL, explicitly
  usable outside Zephyr. This is Nordic's own official answer to "no
  classic SoftDevice on nRF54L" and should be the first thing evaluated
  before assuming a from-scratch HCI/NimBLE bridge is necessary --
  needs a real toolchain/license check (not yet done) but is a strong
  candidate to shortcut the "design our own SDC-linking glue" work
  entirely.
- `h2zero/NimBLE-Arduino` and `apache/mynewt-nimble` remain the
  reference for the host-only-over-HCI split described below, if the
  Nordic Bare Metal S1xx API above turns out not to fit this core's
  existing Arduino-facing design.

## Vendoring done, and what was verified (2026-07-21)

Fetched directly from `nrfconnect/sdk-nrfxlib` (Nordic's own public
repo, not copied from `lolren/nrf54-arduino-core` or any other
third party -- see `extern/nordic_sdc/VERSION.md` for exact commit
SHAs) into `extern/nordic_sdc/`:

- `softdevice_controller/lib/libsoftdevice_controller_multirole.a`
  (hard-float ABI, matching this core's `-mfloat-abi=hard
  -mfpu=fpv5-sp-d16` in the root `Makefile` -- soft-float, which is
  what `lolren/nrf54-arduino-core` uses, would **not** have linked
  correctly against this project's existing float ABI).
- `mpsl/lib/libmpsl.a` and `mpsl/lib/libmpsl_fem_common.a` (same
  hard-float ABI). `libmpsl_fem_common.a` turned out to be required
  even without an external front-end module/PA chip -- see the linker
  findings below; it's not just for boards with an external FEM.
- The public headers (`sdc*.h`, `mpsl*.h`) and the real
  `LicenseRef-Nordic-5-Clause` license files, unmodified.

### Linker findings (ground truth, not assumed)

A minimal test file calling `mpsl_init()` and `sdc_init()` was compiled
with this core's actual flags (`cores/nrf54l` on the include path, same
`-mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16`) and partially
linked (`ld -r`) against the vendored archives to see exactly what
undefined symbols remain -- i.e. exactly what this core has to
implement, with no guessing:

```
mpsl_fem_caps_get                        )
mpsl_fem_disable                         )
mpsl_fem_enable                          )  resolved by adding
mpsl_fem_lna_configuration_clear         )  libmpsl_fem_common.a --
mpsl_fem_lna_configuration_set           )  no application code
mpsl_fem_pa_configuration_clear          )  needed for these.
mpsl_fem_pa_configuration_set            )
mpsl_fem_pa_power_control_set            )
mpsl_fem_tx_power_split                  )
mpsl_fem_utils_available_cc_channels_cache )

mpsl_hwres_dppi_channel_alloc            -- app must implement: allocate
                                             a DPPI channel on the given
                                             DPPIC instance (mirrors the
                                             nrfx_gpiote_channel_alloc()
                                             pattern already used in
                                             cores/nrf54l/wiring_interrupts.c,
                                             but for nrfx's DPPI channel
                                             allocator instead).
mpsl_hwres_ppib_channel_alloc            -- app must implement: same
                                             idea, for PPIB (PPI Bridge)
                                             channels.
mpsl_low_latency_acquire_callback        -- app must implement: called
                                             by MPSL before time-critical
                                             radio work; per mpsl.h,
                                             typically drops NVM/flash
                                             access latency for the
                                             duration.
mpsl_low_latency_release_callback        -- app must implement: the
                                             matching release, called
                                             after time-critical work
                                             (MPSL may skip intermediate
                                             releases for back-to-back
                                             events -- must tolerate
                                             that, e.g. by counting
                                             acquires/releases rather
                                             than treating them as
                                             strictly paired).
```

That's the **entire** application-side C function surface required to
satisfy the linker -- no missing libc, no missing nrfx symbols beyond
what this core already has. Confirmed via `arm-none-eabi-nm -u` on the
partially-linked object, not assumed from documentation.

Also confirmed via `nm` on `libmpsl.a` itself: `MPSL_IRQ_RADIO_Handler`,
`MPSL_IRQ_RTC0_Handler`, `MPSL_IRQ_TIMER0_Handler`, and
`MPSL_IRQ_CLOCK_Handler` are **defined** (exported) by the library, not
things this core needs to write -- the remaining work there is routing
this chip's real vector-table entries to call them (the same
`#define X_IRQHandler Y_IRQHandler`-style aliasing this project's
`nrfx_mdk_fixups.h`/`nrfx_irqs_*` headers already use for GRTC, just
pointed at MPSL's exported names instead of a locally-defined one).

Also confirmed from `mpsl_hwres.h`: on this chip family (`LUMOS_XXAA`,
which covers nRF54L15), **MPSL claims `NRF_TIMER10` exclusively**
(`#define MPSL_TIMER0 NRF_TIMER10`) -- this core's existing peripheral
drivers don't currently touch TIMER10, but this needs to stay true; any
future driver work must treat TIMER10 as MPSL/BLE-reserved once BLE is
wired up.

## Glue functions implemented (2026-07-21)

`cores/nrf54l/mpsl_glue.c` now implements all four functions the linker
enumerated above:

- `mpsl_hwres_dppi_channel_alloc()` / `mpsl_hwres_ppib_channel_alloc()`
  -- real, working per-instance flag32 channel allocators (the same
  `nrfx_flag32_allocator` helper this core's GRTC/GPIOTE drivers already
  use), keyed by matching the passed-in `NRF_DPPIC_Type*`/`NRF_PPIB_Type*`
  pointer against this chip's real instances (`NRF_DPPIC00/10/20/30`,
  `NRF_PPIB00/01/10/11/20/21/22/30`). Channel counts per instance are
  read from `nrf54l15_application_peripherals.h`
  (`DPPICxx_CH_NUM_MAX`/`PPIBxx_NTASKSEVENTS_MAX`), not assumed.
  Deliberately does **not** use this project's vendored
  `extern/nrfx/helpers/nrfx_gppi*` subsystem -- that's a much larger
  cross-domain DPPI<->PPIB routing/connection system this core doesn't
  otherwise need; MPSL's contract is just "give me a channel on this one
  instance," which a small standalone allocator satisfies directly.
- `mpsl_low_latency_acquire_callback()` / `_release_callback()` -- **left
  as no-op stubs on purpose, not a real implementation.** mpsl.h's own
  doc comment gives "placing the NVM controller in low-latency mode" as
  only an *example* of what these might do, not a mandated mechanism.
  The plausible real mechanism on this chip is
  `nrf_lrcconf_poweron_force_set()` (`extern/nrfx/hal/nrf_lrcconf.h`,
  the same LRCCONF peripheral noted in `docs/LOW_POWER_ROADMAP.md`), but
  *which* power domain(s) SDC/MPSL actually need forced active during
  radio activity on the nRF54L15 isn't documented in these vendored
  headers -- guessing wrong here would be worse than a no-op (a wrong
  domain mask could silently power down something SDC assumes is
  active, versus a no-op only costing timing margin/power efficiency).
  Needs the real answer from Nordic's own MPSL/NCS reference
  integration before this stops being a stub.

**Verified by linking for real**, the same way the four functions were
originally identified: recompiled the minimal `mpsl_init()`/`sdc_init()`
test file, partially linked it (`ld -r`) against the real vendored SDC +
MPSL + MPSL-FEM-common archives plus `mpsl_glue.o` and
`nrfx_flag32_allocator.o`, and confirmed with `arm-none-eabi-nm -u` that
**zero** undefined symbols remain -- full, real symbol resolution, not
assumed.

Because `mpsl_glue.c` now lives in `cores/nrf54l/` (required so Arduino
IDE/`arduino-cli` auto-discovers it -- unlike this project's `Makefile`,
which lists core sources explicitly), it's compiled into *every* sketch
build now, including ones that never touch BLE. Confirmed this doesn't
regress anything: `platform.txt`'s `compiler.includes` and the
`Makefile`'s `INCLUDES` both gained the two `extern/nordic_sdc/*/include`
paths (needed for `mpsl_glue.c` to compile at all), and a full manual
Blink rebuild with `mpsl_glue.c` included still links to the same size
as before -- `--gc-sections` correctly strips the four unused functions
out of sketches that never call `mpsl_init()`/`sdc_init()`, so there's
no dead-weight cost yet.

## GRTC/MPSL conflict: resolved (2026-07-21)

**Answer: there is no conflict, by chip design -- MPSL and this core's
own GRTC usage already sit on separate channels *and* separate physical
interrupt lines.** Found in Nordic's own MPSL integration notes
(nrfxlib docs, "MPSL nRF54L Series Integration Requirements"), not
guessed:

- **MPSL reserves GRTC channels 7-11, routed to interrupt `GRTC_3_IRQn`**
  specifically, for the nRF54L series (plus `TIMER10` and `TIMER20`).
  The GRTC peripheral splits its channels across four independent NVIC
  lines (`GRTC_0..3_IRQn`) precisely so a subsystem like MPSL can own a
  channel range and its own vector without disturbing other GRTC users
  on the same physical peripheral.
- **This core's own GRTC vector is `GRTC_2_IRQn`, not `GRTC_3_IRQn`.**
  Confirmed by reading `extern/nrfx/bsp/stable/soc/nrfx_mdk_fixups.h`'s
  `NRF54L15_XXAA` block directly: for `NRF_APPLICATION` builds without
  `NRF_TRUSTZONE_NONSECURE` defined (this core's actual build, per its
  secure-only v1 scoping -- see `docs/ARCHITECTURE.md`),
  `GRTC_IRQn`/`GRTC_IRQHandler` resolve to `GRTC_2_IRQn`/
  `GRTC_2_IRQHandler`, and `GRTC_MAIN_CC_CHANNEL` (the channel
  `nrfx_grtc_syscounter_start()` auto-allocates for `millis()`/
  `micros()`) is channel 0. `delay()`'s System ON idle wake channel
  (`docs/LOW_POWER_ROADMAP.md`) is channel 1, the next one
  `nrfx_grtc_channel_alloc()` hands out. Both are on `GRTC_2_IRQn`,
  nowhere near MPSL's `GRTC_3_IRQn`/channels 7-11.
- **A real, previously-latent bug was found and fixed while confirming
  this**, though: this core's `NRFX_GRTC_CONFIG_ALLOWED_CC_CHANNELS_MASK`
  was nrfx's generic template default, `0x00000f0f` -- channels
  `{0,1,2,3,8,9,10,11}`. Channels 8-11 of that set directly overlap
  MPSL's reserved 7-11 range. Not a live bug today (this core has only
  ever allocated channels 0 and 1), but a future GRTC-based feature
  could have silently allocated channel 8-11 and collided with MPSL the
  moment BLE was wired up. Narrowed to `0x0000000f` (channels 0-3 only)
  in `cores/nrf54l/nrfx_config_nrf54l15_application.h` -- two channels
  of headroom beyond this core's current two users, safely clear of
  MPSL's range. Rebuilt `Blink` to confirm no regression.

This resolves the "does GRTC-based `millis()`/`delay()` conflict with
BLE" question this document previously flagged as blocking IRQ vector
work. It does **not** mean IRQ vector routing is done -- see the next
step.

## IRQ vector routing implemented (2026-07-21)

`cores/nrf54l/mpsl_glue.c` now defines the four vector-forwarding
functions, confirmed against Nordic's own real reference integration
source (`nrfconnect/sdk-nrf`, `subsys/mpsl/init/include/mpsl_init_soc.h`
and `subsys/mpsl/init/mpsl_init.c`, plus
`drivers/mpsl/clock_control/nrfx_clock_mpsl.c` for the clock vector),
not guessed:

- `RADIO_0_IRQHandler()` -> `MPSL_IRQ_RADIO_Handler()`. **`RADIO_1` is
  deliberately untouched** -- confirmed from `mpsl_init_soc.h` that
  `MPSL_RADIO_IRQn = RADIO_0_IRQn` only.
- `GRTC_3_IRQHandler()` -> `MPSL_IRQ_RTC0_Handler()` (`MPSL_RTC_IRQn`).
- `TIMER10_IRQHandler()` -> `MPSL_IRQ_TIMER0_Handler()` (`MPSL_TIMER_IRQn`).
- `CLOCK_POWER_IRQHandler()` -> `MPSL_IRQ_CLOCK_Handler()` (found in a
  different file than the other three -- sdk-nrf's own clock control
  driver, not `mpsl_init.c`).

Confirmed from the same reference that each real Zephyr wrapper does
*only* the one forwarding call, no extra logic -- so a plain one-line
forwarding function is the correct, complete implementation, not a
simplification of something more involved.

**A second real bug was caught by actually linking, the same way the
first pass caught the missing glue functions**: unlike
`mpsl_hwres_dppi_channel_alloc()` and friends (only reachable by a
function *call*, so `--gc-sections` strips them from builds that never
call in), these four `*_IRQHandler` functions are referenced by the
vector table itself (`g_pfnVectors` in the vendored MDK startup `.S`,
which is never garbage-collected) the moment they're *defined* --
regardless of whether anything calls them. Defining them
unconditionally in `mpsl_glue.c` (which lives in `cores/nrf54l/` and is
therefore compiled into every sketch) broke a plain `Blink` build with
"undefined reference to `MPSL_IRQ_RADIO_Handler`" and friends, since
Blink correctly never links `libmpsl.a`. Fixed by gating all four
behind `#if defined(ARDUINO_NRF54_MPSL_ENABLED)` (undefined by
default) -- this project has no "opt-in library" mechanism for BLE yet
(a real Arduino library, only compiled when a sketch `#include`s it,
would be the cleaner long-term home for this instead of a macro gate in
`cores/nrf54l/`; revisit once BLE has an actual Arduino-facing library).

**Verified both directions for real:**
- Rebuilt `Blink` with the flag *undefined* (the default) -- links
  clean, same size as before, confirming no regression for ordinary
  sketches.
- Compiled `mpsl_glue.c` with the flag *defined*, partially linked
  against the real vendored SDC + MPSL + MPSL-FEM-common archives, and
  confirmed **zero undefined symbols** via `arm-none-eabi-nm -u`.
  Disassembled the result and confirmed `RADIO_0_IRQHandler` genuinely
  calls into the real, linked `MPSL_IRQ_RADIO_Handler` address -- not
  just "links," actually wired to the right function.

Still NOT done: nothing calls `mpsl_init()`/`sdc_init()`/
`sdc_enable()` anywhere in this repo, so these vectors are still inert
in practice (never enabled at the NVIC, never fired). That's the next
step.

## Concrete next steps

1. ~~Implement the four glue functions~~ **Done** -- see above.
2. ~~Route this chip's real IRQ vectors~~ **Done** -- see "IRQ vector
   routing implemented" above.
3. Get `mpsl_init()` + `sdc_init()` + `sdc_enable()` actually running on
   real hardware (build-and-link success is not the same as working --
   this needs the same over-SWD verification discipline used for every
   other peripheral in this core, see `docs/VERIFICATION.md`). Confirmed
   call ordering from the same real reference: `mpsl_init()` is called
   *first*; only afterward are `RADIO_0`/`GRTC_3`/`TIMER10` connected and
   enabled at the NVIC, at priority 0 (`MPSL_HIGH_IRQ_PRIORITY`) -- this
   core's `main.cpp` (which already runs `nrf54_core_time_init()` before
   `setup()`) is the natural place for this sequence, gated behind
   `ARDUINO_NRF54_MPSL_ENABLED` and only once `sdc_enable()` needs it.
4. Evaluate Nordic's own **Bare Metal S115/S145** SoftDevice-style API
   (see reference below) as a possible shortcut for the *host* side
   before building a NimBLE+HCI bridge from scratch -- it sits on top
   of the same SDC/MPSL vendored above, so steps 1-3 aren't wasted work
   either way.
5. If the Bare Metal S1xx option doesn't fit this core's design: scope
   NimBLE's host-only build flags/config needed to disable its built-in
   nRF52 controller and run purely over HCI, then design the
   in-process HCI transport shim between NimBLE-host and SDC.
6. Only after the above: design the actual Arduino-facing API (likely
   modeled on `ArduinoBLE`'s API surface for familiarity, similar to how
   this core's other peripherals mirror standard Arduino APIs).

## Low-power sleep modes (tracked separately)

See `docs/LOW_POWER_ROADMAP.md` -- System ON idle for `delay()` is now
implemented (compile/link-verified, not yet hardware-verified); System
OFF deep sleep is still just scoped.
