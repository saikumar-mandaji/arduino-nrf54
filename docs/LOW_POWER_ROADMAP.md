# Low-power sleep modes: scoping notes + System ON idle for delay()

This document records the design pass for adding real low-power sleep
to this core, plus the status of the first implemented piece.

**Status (2026-07-21): `delay(ms)` now sleeps via System ON idle
(WFI + a dedicated GRTC compare channel) instead of busy-waiting.**
See `cores/nrf54l/wiring_time.c`. This compiles and links cleanly
against the real `nrfx_grtc` driver source (verified by manually
replicating the Makefile's build recipe for `Blink` end-to-end), but
is **not yet hardware-validated** -- next step is halting the CPU over
SWD mid-`delay()` on a real nRF54L15-DK to confirm it's actually
sitting in WFI and not spinning, the same technique used to catch the
GRTC autostart and NULL-pointer bugs documented in
`docs/VERIFICATION.md`. `delayMicroseconds()` intentionally still
busy-waits (see the rationale in the `wiring_time.c` file comment) --
only `delay()`'s millisecond-granularity sleeps get the System ON idle
treatment, since the interrupt/wake overhead would make short
`delayMicroseconds()` calls inaccurate.

Everything below this point is the original scoping pass, kept for the
still-unimplemented System OFF (deep sleep) tier. This is the smaller,
more tractable counterpart to `docs/BLE_ROADMAP.md` -- no precompiled
vendor binaries involved, everything needed is already in the vendored
`nrfx`/CMSIS headers.

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

## Reference repo found (2026-07-21)

`lolren/nrf54-arduino-core` (GitHub, MIT -- see `docs/BLE_ROADMAP.md`'s
"Reference repos found" section for how it was verified as a real,
actively-released repo rather than trusting its README at face value)
ships a comparable idle/sleep design for the same chip family (XIAO
nRF54L15/LM20A). Concretely useful, verified-by-reading-source parts:

- **A peripheral-driver idle-service hook pattern.** Its
  `cores/nrf54l15/idle_service.cpp` (real file, read directly) defines
  `nrf54l15_clean_idle_service()`/`nrf54l15_clean_yield_service()` as a
  dispatcher that each peripheral driver (analog-write, serial, SPI,
  Wire, optionally BLE via a weak symbol) hooks into, gated by an
  `NRF54L15_CLEAN_AUTO_GATE` compile flag. This is a directly reusable
  pattern for next step #2 below: rather than auditing every driver
  ad hoc, give this core the same kind of central idle-hook dispatcher
  so each peripheral registers its own "is it safe to sleep now"
  check/gate instead of `delay()` needing to know about every driver.
- **Example sketch names worth reading before designing our own API**
  (`libraries/Nrf54L15-Clean-Implementation/examples/LowPower/`):
  `LowPowerIdleWfi`, `LowPowerDelaySystemOff`, `LowPowerGrtcPwmSystemOff`,
  `LowPowerCpuFrequencyControl`, `LowPowerDutyCycleAdc`,
  `LowPowerPeripheralGating`, `InterruptWatchdogLowPower` -- these are
  real example filenames pulled from the repo's git tree, useful as a
  checklist of the kinds of scenarios a `LowPower` API should cover
  (plain WFI idle, timed System OFF, GRTC-driven wake, CPU frequency
  scaling, ADC duty-cycling, peripheral clock gating), not yet verified
  line-by-line for correctness.
- **Same caveat as the BLE roadmap**: this repo's README claims specific
  measured current numbers (e.g. an nPM1300 hibernate path reaching
  "~0.5 uA") and a large feature surface; treat those as claims to
  independently re-measure on our own hardware, not facts to cite,
  consistent with this project's practice of only trusting numbers we
  measured ourselves (see `docs/VERIFICATION.md`).

## Concrete next steps

1. ~~Prototype System ON idle first~~ **Implemented (compile-verified,
   not yet hardware-verified)** -- `delay(ms)` now arms a dedicated
   GRTC compare channel and sleeps in a `while (!s_delay_woken) __WFI();`
   loop instead of busy-waiting; see the status note at the top of this
   file. Still need to: validate on real hardware over SWD the same way
   GRTC/SPI bugs were found earlier in this project -- halt the CPU
   mid-`delay()` and confirm it's actually in a WFI-halted state, not
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
