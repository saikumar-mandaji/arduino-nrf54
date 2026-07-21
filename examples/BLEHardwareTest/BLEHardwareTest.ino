/*
  BLEHardwareTest -- hardware verification for the SDC/MPSL bring-up
  sequence (mpsl_init() -> sdc_init() -> sdc_rand_source_register() ->
  sdc_enable()) implemented in cores/nrf54l/mpsl_glue.c. See
  docs/BLE_ROADMAP.md.

  This does NOT test any actual BLE functionality (no HCI host exists
  in this core yet) -- it only confirms the controller bring-up
  sequence itself succeeds on real silicon, which the linker cannot
  verify. Requires building with -DARDUINO_NRF54_MPSL_ENABLED and
  linking extern/nordic_sdc's vendored SDC/MPSL archives (not part of
  the normal Makefile/platform.txt build yet -- see the Makefile
  invocation used to build this in docs/VERIFICATION.md).

  Verified on a real nRF54L15-DK (2026-07-21): LED_BUILTIN lights up
  solid (not blinking) when nrf54_mpsl_ble_init() returns true, read
  back over SWD as bleInitResult == 1 -- confirmed the full sequence
  (mpsl_init, RADIO/GRTC/TIMER10 IRQ setup, nrfx_cracen_init,
  sdc_init, sdc_rand_source_register, sdc_cfg_set, malloc, sdc_enable)
  genuinely succeeds on real hardware, not just compiles/links.
*/
#include <Arduino.h>

extern "C" bool nrf54_mpsl_ble_init(void);

volatile int bleInitResult = -1;

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    bool ok = nrf54_mpsl_ble_init();
    bleInitResult = ok ? 1 : 0;
}

void loop()
{
#if LED_BUILTIN_ACTIVE_LOW
    digitalWrite(LED_BUILTIN, bleInitResult == 1 ? LOW : HIGH);
#else
    digitalWrite(LED_BUILTIN, bleInitResult == 1 ? HIGH : LOW);
#endif
    delay(200);
}
