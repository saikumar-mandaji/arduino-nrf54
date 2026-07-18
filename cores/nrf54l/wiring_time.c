/**
 * @file wiring_time.c
 * @brief millis()/micros()/delay()/delayMicroseconds() on top of GRTC.
 *
 * The GRTC SYSCOUNTER runs at NRF_GRTC_SYSCOUNTER_MAIN_FREQUENCY_HZ (1 MHz
 * -- see extern/nrfx/hal/nrf_grtc.h), so one tick is exactly one
 * microsecond and no scaling math is needed for micros().
 *
 * NRFX_GRTC_CONFIG_AUTOEN/AUTOSTART are enabled in
 * nrfx_config_nrf54l15_application.h, which per nrfx's own config
 * documentation was expected to bring the counter up running from
 * nrfx_grtc_init() alone -- on real nRF54L15-DK hardware this did NOT
 * happen: delay() hung forever on its first call (confirmed by halting
 * the CPU over SWD mid-hang and finding it stuck in
 * nrfx_grtc_syscounter_get(), called from micros(), which was returning
 * a value that never advanced). Explicitly starting the syscounter with
 * nrfx_grtc_syscounter_start() fixes it. AUTOEN/AUTOSTART are left
 * enabled in the config (harmless) but are no longer trusted alone.
 */
#include "Arduino.h"
#include <nrfx_grtc.h>
#include <hal/nrf_grtc.h>

#define GRTC_IRQ_PRIORITY 7

void nrf54_core_time_init(void)
{
    (void)nrfx_grtc_init(GRTC_IRQ_PRIORITY);

    /* busy_wait=true: block here until the syscounter is confirmed
     * actually running, rather than assuming AUTOSTART already did it.
     * p_main_cc_channel must point at real storage -- confirmed on real
     * hardware that nrfx_grtc's internal channel_allocate() writes
     * through this pointer unconditionally with no NULL check, so
     * passing NULL here (as its signature's "optional output" framing
     * suggested) causes a precise BusFault (NULL pointer store),
     * escalated to HardFault since fault handlers aren't individually
     * enabled. Caught by halting the CPU over SWD and reading the
     * exception stack frame; see docs/VERIFICATION.md. */
    static uint8_t s_grtc_main_cc_channel;
    (void)nrfx_grtc_syscounter_start(true, &s_grtc_main_cc_channel);
}

uint32_t micros(void)
{
    return (uint32_t)nrfx_grtc_syscounter_get();
}

uint32_t millis(void)
{
    return micros() / 1000UL;
}

void delayMicroseconds(uint32_t us)
{
    uint32_t start = micros();
    while ((micros() - start) < us)
    {
        /* busy-wait; relies on micros() wraparound-safe unsigned subtraction */
    }
}

void delay(uint32_t ms)
{
    while (ms > 0)
    {
        delayMicroseconds(1000);
        ms--;
    }
}
