/**
 * @file nrfx_config.h
 * @brief Top-level nrfx configuration entry point for arduino-nrf54.
 *
 * nrfx itself does not ship this file -- it's the integrator's
 * responsibility (see nrfx's README "Integration notes"). This wrapper
 * exists only to satisfy the `#ifndef NRFX_CONFIG_H__` guard at the top of
 * Nordic's per-chip config template before including it; the actual option
 * values live in nrfx_config_nrf54l15_application.h, which is a vendored
 * copy of extern/nrfx/bsp/stable/templates/nrfx_config_nrf54l15_application.h
 * with only NRFX_UARTE_ENABLED and NRFX_GRTC_ENABLED flipped on.
 *
 * REAL BUG FOUND AND FIXED ON HARDWARE (2026-07-21): nrfx's own
 * nrfx_templates_config.h has `//#include <soc/nrfx_irqs.h>` --
 * commented out, left for the integrator to add. This project never
 * added it. Without it, every nrfx driver's generic
 * `nrfx_<periph>_irq_handler` function name is never renamed (via the
 * per-chip macro chain in soc/irqs/nrfx_irqs_nrf54l15_application.h)
 * to the real chip vector name (e.g. `GRTC_2_IRQHandler`) -- the driver
 * function just compiles under its generic name, unused and
 * unreferenced, while the real vector table slot stays wired to the
 * weak `Default_Handler` (an infinite `b .` loop).
 *
 * This was completely invisible to every build/link check performed
 * throughout this project's development, including this session's own
 * (`arm-none-eabi-nm -u` only catches *undefined* symbols -- a weak
 * symbol silently aliased to Default_Handler is not undefined, so it
 * never showed up as an error). It was only caught by actually halting
 * a real nRF54L15-DK over SWD while running `delay()` (the new System
 * ON idle code from docs/LOW_POWER_ROADMAP.md, which was the first
 * code in this whole project to actually depend on the GRTC interrupt
 * firing -- the old busy-wait delay() never needed the interrupt to
 * work, so this bug had no way to surface before now) and finding the
 * CPU permanently stuck at the exact same PC after repeated
 * halt/resume, inside what `nm` confirmed was `Default_Handler`
 * (shared address with the weak, never-overridden `GRTC_2_IRQHandler`),
 * while `nrfx_grtc_irq_handler` sat compiled-but-unused at a different
 * address entirely. See docs/VERIFICATION.md.
 *
 * This one include fixes the rename chain for every nrfx-driven
 * interrupt in this core at once (GRTC, GPIOTE, UARTE, SPIM, TWIM,
 * SAADC, PWM) -- not just the one this session happened to catch.
 */
#ifndef NRFX_CONFIG_H__
#define NRFX_CONFIG_H__

#include "nrfx_config_nrf54l15_application.h"
#include <soc/nrfx_irqs.h>

#endif /* NRFX_CONFIG_H__ */
