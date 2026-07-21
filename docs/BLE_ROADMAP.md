# BLE: scoping notes (not implemented yet)

This document records the research done so far into adding BLE support
to this core. Nothing described here is implemented -- this is a design
scoping pass, written so the next work session (or a contributor) isn't
starting from zero.

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

## Concrete open questions for the next session

1. ~~Confirm the actual Nordic license text in `sdk-nrfxlib` permits
   redistributing the compiled SDC/MPSL `.a` files~~ **Resolved** --
   see the `lolren/nrf54-arduino-core` finding above:
   `LicenseRef-Nordic-5-Clause` permits binary redistribution.
2. ~~Get a real SDC+MPSL prebuilt lib for the nRF54L15 in hand and
   confirm it links against a non-Zephyr, non-NCS build~~ **Largely
   resolved by existence proof** -- `lolren/nrf54-arduino-core` vendors
   exactly this and (per its release history) ships working builds.
   Still need to independently obtain the same libs ourselves (they're
   in `sdk-nrfxlib`, not this repo's own IP) and confirm they link
   against *our* build, not just trust that theirs does.
3. Evaluate Nordic's own **Bare Metal S115/S145** option (see reference
   above) as a possible shortcut before building a NimBLE+HCI bridge --
   check its toolchain assumptions and license against this core's
   actual GCC/Makefile build.
4. Scope NimBLE's host-only build flags/config needed to disable its
   built-in nRF52 controller and run purely over HCI (still needed if
   the Bare Metal S1xx option doesn't fit).
5. Design the in-process HCI transport shim between NimBLE-host and
   SDC (still needed if going the NimBLE route instead of S1xx).
6. Only after the above are unblocked: design the actual Arduino-facing
   API (likely modeled on `ArduinoBLE`'s API surface, for familiarity
   with existing Arduino BLE code, similar to how this core's other
   peripherals mirror standard Arduino APIs).

## Low-power sleep modes (tracked separately, smaller scope)

Not yet researched in depth. Currently `delay()` busy-waits on GRTC
compare events rather than entering any real sleep state. The nRF54L15
supports multiple System OFF/ON idle states via its power management
unit; adding a real low-power `delay()`/idle path is a smaller, more
tractable project than BLE and doesn't have the "precompiled vendor
blob" complication -- next session should scope this before BLE if a
smaller, shippable win is wanted first.
