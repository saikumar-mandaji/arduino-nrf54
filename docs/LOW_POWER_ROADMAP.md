# Low-power sleep modes: scoping notes (not implemented yet)

This document records the design pass for adding real low-power sleep
to this core. Nothing described here is implemented -- `delay()` today
(`cores/nrf54l/wiring_time.c`) busy-waits by polling `micros()` in a
tight loop, which holds the CPU (and everything clocked off it) fully
active for the entire delay. This is the smaller, more tractable
counterpart to `docs/BLE_ROADMAP.md` -- no precompiled vendor binaries
involved, everything needed is already in the vendored `nrfx`/CMSIS
headers.

## What the nRF54L15 actually offers

Confirmed by reading the vendored headers in `extern/nrfx` (not
guessed):

- **No classic `POWER` peripheral SYSTEMOFF**, unlike the nRF52 series.
  `extern/nrfx/hal/nrf_power.h`'s `nrf_power_system_off()` is compiled
  out on this chip (guarded by `POWER_SYSTEMOFF_SYSTEMOFF_Enter`, which
  the nRF54L15's peripheral header doesn't define).
- **System OFF is instead reached via the `REGULATORS` peripheral**:
  `extern/nrfx/hal/nrf_regulators.h` has `nrf_regulators_system_off()`,
  guarded by `NRF_REGULATORS_HAS_SYSTEMOFF` (which the nRF54L15 *does*
  define). This is the real, chip-correct API to call for a full deep
  sleep.
- **Per-domain power control is via `LRCCONF`** (Local domain Register
  Clock CONFiguration, `extern/nrfx/hal/nrf_lrcconf.h`), which is new
  relative to the nRF52 generation -- it lets each local power domain
  (CPU core, radio, peripherals) independently request to stay active
  or drop to a retained/low-power state, rather than one chip-wide
  POWER peripheral controlling everything.
- **GRTC has an explicit "keep SYSCOUNTER active" request API**
  (`nrf_grtc_syscounter_active_request_set()` and the per-domain
  variant in `nrf_grtc.h`). This matters directly for `delay()`/
  `millis()`: GRTC lives in its own power domain and can itself drop to
  a lower-power state independent of the CPU, so a sleep-based `delay()`
  needs to explicitly request GRTC stay active (or rely on its compare
  channel's wake capability) rather than assuming it always ticks.

## Two distinct sleep tiers to design for

Arduino cores that expose low-power modes (e.g. ESP32's
`esp_light_sleep_start()`/`esp_deep_sleep_start()`) generally split into
a "cheap, fast-resume" tier and a "deep, slow-resume" tier. The nRF54L15
maps onto the same split:

1. **System ON idle (light sleep)** -- CPU executes `WFE`/`WFI` and
   halts until an interrupt/event, but RAM and peripheral state are
   retained and resume is fast (no reboot). This is the natural
   replacement for `delay()`'s busy-wait: instead of polling
   `micros()`, arm a GRTC compare-channel interrupt for the target
   time and `WFE` until it fires. Requires: keeping GRTC's SYSCOUNTER
   active per the note above, and auditing every other peripheral
   driver in this core (UARTE, SPIM, TWIM, SAADC, PWM, GPIOTE) for
   whether an in-flight transfer would be corrupted by the CPU
   sleeping mid-operation -- DMA-driven peripherals (which nrfx uses
   throughout) should be fine since DMA runs independent of the CPU,
   but this needs verifying per-peripheral, not assumed.
2. **System OFF (deep sleep)** -- via
   `nrf_regulators_system_off()`. RAM and CPU state are **not**
   retained (a wake is a full reboot from reset, not a return from the
   function call), so this needs an explicit, separate API (e.g. a
   `sleep()`/`deepSleep()` function, not folded into `delay()`) with
   a real wake-source configuration story before it's useful: GPIO
   sense (wake on pin state change) is the obvious first wake source
   to support, modeled on which pins/edges are configured before
   entering System OFF. Retained-across-reset general-purpose
   registers (if the nRF54L15 has an equivalent to nRF52's GPREGRET)
   need identifying for passing a "why did we wake up" reason to
   `setup()`, matching the pattern ESP32's `esp_sleep_get_wakeup_cause()`
   gives users.

## Concrete next steps

1. Prototype System ON idle first (smaller, self-contained, no new
   public API needed -- just make `delay()`/an internal idle path
   sleep instead of busy-wait) and validate on real hardware over SWD
   the same way GRTC/SPI bugs were found this session: halt the CPU
   mid-`delay()` and confirm it's actually in a WFE-halted state, not
   spinning.
2. Audit each existing peripheral driver (`HardwareSerial`, `SPI`,
   `Wire`, `analogRead`, `analogWrite`, `attachInterrupt`) for sleep
   interaction -- specifically, whether an ISR needs to run to
   completion or a DMA transfer needs to finish before it's safe to
   re-enter idle, and whether GPIOTE-based `attachInterrupt` pins can
   themselves serve as System ON wake sources (they should, being
   event-driven, but needs confirming against real hardware since this
   project has repeatedly found nrfx's documented behavior not to match
   silicon on this chip -- see `docs/VERIFICATION.md`).
3. Design and implement System OFF as a separate opt-in API
   (`LowPower.sleep(wakePin, mode)`-style, name TBD) once GPIO wake
   sources are confirmed working, including documenting the "your RAM
   is gone, `setup()` runs again" semantics clearly since this differs
   from System ON idle and from what `delay()` users would expect.
4. Document real current-draw numbers once both tiers are implemented
   and measurable on the real nRF54L15-DK (this core has consistently
   preferred measured numbers over datasheet claims throughout hardware
   bring-up -- keep that practice here).
