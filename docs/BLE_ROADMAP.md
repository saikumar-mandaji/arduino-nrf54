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

## Concrete open questions for the next session

1. Confirm the actual Nordic license text in `sdk-nrfxlib` permits
   redistributing the compiled SDC/MPSL `.a` files inside this MIT
   repo's release archives (same place `tools/make_release.sh` already
   vendors pruned `nrfx`/`CMSIS_6`).
2. Get a real SDC+MPSL prebuilt lib for the nRF54L15 in hand and
   confirm it links against our existing (non-Zephyr, non-NCS) build
   -- this is the single biggest unknown, since every public example
   of SDC standalone-without-Zephyr usage found so far still assumes
   an NCS-adjacent build environment we don't have.
3. Scope NimBLE's host-only build flags/config needed to disable its
   built-in nRF52 controller and run purely over HCI.
4. Design the in-process HCI transport shim between NimBLE-host and
   SDC.
5. Only after 1-4 are unblocked: design the actual Arduino-facing API
   (likely modeled on `ArduinoBLE`'s API surface, for familiarity with
   existing Arduino BLE code, similar to how this core's other
   peripherals mirror standard Arduino APIs).

## Low-power sleep modes (tracked separately, smaller scope)

Not yet researched in depth. Currently `delay()` busy-waits on GRTC
compare events rather than entering any real sleep state. The nRF54L15
supports multiple System OFF/ON idle states via its power management
unit; adding a real low-power `delay()`/idle path is a smaller, more
tractable project than BLE and doesn't have the "precompiled vendor
blob" complication -- next session should scope this before BLE if a
smaller, shippable win is wanted first.
