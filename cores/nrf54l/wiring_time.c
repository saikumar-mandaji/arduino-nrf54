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
 *
 * delay(ms) -- System ON idle (see docs/LOW_POWER_ROADMAP.md)
 * -------------------------------------------------------------
 * delay() arms a dedicated GRTC compare channel for the target wake
 * time and then executes WFI in a loop instead of busy-polling
 * micros(), so the CPU is actually halted (not spinning) for the
 * majority of the delay. This is safe with WFI specifically (not just
 * "sleep and hope"): the wake flag is cleared *before* arming the
 * compare channel and interrupts are re-enabled immediately after, so
 * if the compare event already fired during that brief window the flag
 * is already true by the time the `while (!s_delay_woken)` check runs
 * and WFI is skipped entirely -- there is no missed-wakeup window.
 * Any other enabled interrupt (GPIOTE, UARTE, etc.) also wakes the CPU
 * harmlessly; the loop just rechecks the flag and goes back to WFI if
 * it's not yet time.
 *
 * delayMicroseconds() intentionally still busy-waits: arming a GRTC
 * channel and taking an interrupt has overhead on the order of at least
 * a few microseconds, which would make short delayMicroseconds() calls
 * (as used for bit-banged/software timing in user sketches) inaccurate
 * or even shorter than the call overhead itself. delay(ms) doesn't have
 * that problem since millisecond-granularity sleeps dwarf the wake
 * latency.
 *
 * NOT YET HARDWARE-VALIDATED. This has been reasoned through against
 * the real GRTC driver source (extern/nrfx/drivers/src/nrfx_grtc.c) and
 * compiles, but per this project's own practice of only trusting what's
 * been confirmed on real silicon (see docs/VERIFICATION.md), the actual
 * CPU-halted-during-delay() behavior still needs confirming over SWD on
 * a real nRF54L15-DK: halt the CPU mid-delay() and check it's sitting in
 * WFI (sleeping) rather than spinning, the same technique that caught
 * the GRTC autostart and NULL-pointer bugs above.
 */
#include "Arduino.h"
#include <nrfx_grtc.h>
#include <hal/nrf_grtc.h>

#define GRTC_IRQ_PRIORITY 7

static uint8_t s_delay_channel;
static bool s_delay_channel_ready = false;
static volatile bool s_delay_woken = false;

static void delay_wake_handler(int32_t id, uint64_t cc_value, void * p_context)
{
    (void)id;
    (void)cc_value;
    (void)p_context;
    s_delay_woken = true;
}

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

    /* Dedicated compare channel used only to wake delay()'s WFI loop.
     * channel_callback_set() both registers the handler and enables the
     * per-channel interrupt (see nrfx_grtc.c) -- no separate
     * cc_int_enable() call needed. If no channel is free, s_delay_channel_ready
     * stays false and delay() falls back to the old busy-wait below. */
    if (nrfx_grtc_channel_alloc(&s_delay_channel) == 0)
    {
        nrfx_grtc_channel_callback_set(s_delay_channel, delay_wake_handler, NULL);
        s_delay_channel_ready = true;
    }
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

static void delay_busy_wait_fallback(uint32_t ms)
{
    while (ms > 0)
    {
        delayMicroseconds(1000);
        ms--;
    }
}

void delay(uint32_t ms)
{
    if (ms == 0)
    {
        return;
    }

    if (!s_delay_channel_ready)
    {
        delay_busy_wait_fallback(ms);
        return;
    }

    uint64_t target = nrfx_grtc_syscounter_get() + ((uint64_t)ms * 1000ULL);

    s_delay_woken = false;

    NRFX_CRITICAL_SECTION_ENTER();
    /* safe_setting=true: per the API's own docs this guards against a
     * stale compare event from this channel's previous use being
     * misread as "already expired" for the new target. */
    nrfx_grtc_syscounter_cc_abs_set(s_delay_channel, target, true);
    NRFX_CRITICAL_SECTION_EXIT();

    while (!s_delay_woken)
    {
        __WFI();
    }
}
