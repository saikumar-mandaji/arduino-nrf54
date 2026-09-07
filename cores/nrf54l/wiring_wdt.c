/**
 * @file wiring_wdt.c
 * @brief Implementation of the watchdog wrapper declared in wiring_wdt.h.
 *
 * Uses WDT31, the application-core-accessible watchdog instance --
 * confirmed via extern/nrfx/bsp/stable/mdk/nrf54l/nrf54l15/nrf54l15_global.h
 * (NRF_WDT31 resolves for this core; NRF_WDT30 is the Secure-only
 * instance) and cross-checked against Zephyr's own board devicetree
 * (`watchdog0 = &wdt31;` in both the Ezurio and DK reference designs).
 *
 * NRFX_WDT_CONFIG_NO_IRQ is 0 (this project's config default), so
 * nrfx_wdt_init() requires a real event handler function pointer, and
 * per this project's own established convention (see
 * docs/VERIFICATION.md's "five peripheral drivers need integrator-
 * provided IRQ trampolines" finding), a named IRQ trampoline function
 * is required for the vector table to actually reach it -- WDT31's
 * vector name follows the same per-chip macro chain as GRTC/SAADC.
 */
#include "wiring_wdt.h"

#include <stddef.h>

#include <nrfx_wdt.h>

static nrfx_wdt_t s_wdt31 = NRFX_WDT_INSTANCE(NRF_WDT31);
static bool s_wdt_running = false;

static void wdt_event_handler(nrf_wdt_event_t event_type, uint32_t requests, void *p_context)
{
    (void)event_type;
    (void)requests;
    (void)p_context;
    /* WDT31 fires this ~2 GRTC ticks (per this chip's fixed WDT lock
     * window) before the actual reset -- too little time to do anything
     * useful in a generic core wrapper. Left empty deliberately: a
     * sketch that wants a pre-reset hook should call nrfx_wdt_init()
     * itself instead of using this simplified wrapper. */
}

void nrfx_wdt_31_irq_handler(void)
{
    nrfx_wdt_irq_handler(&s_wdt31);
}

bool wdt_begin(uint32_t timeout_ms)
{
    if (s_wdt_running) {
        return false;
    }

    nrfx_wdt_config_t config = NRFX_WDT_DEFAULT_CONFIG;
    config.reload_value = timeout_ms;

    if (nrfx_wdt_init(&s_wdt31, &config, wdt_event_handler, NULL) != 0) {
        return false;
    }

    nrfx_wdt_channel_id channel_id;
    if (nrfx_wdt_channel_alloc(&s_wdt31, &channel_id) != 0) {
        return false;
    }

    nrfx_wdt_enable(&s_wdt31);
    s_wdt_running = true;
    return true;
}

void wdt_feed(void)
{
    if (s_wdt_running) {
        nrfx_wdt_feed(&s_wdt31);
    }
}
