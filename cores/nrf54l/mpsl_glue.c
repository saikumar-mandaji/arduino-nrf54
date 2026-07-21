/**
 * @file mpsl_glue.c
 * @brief Application-side glue functions required to link Nordic's
 * vendored MPSL (extern/nordic_sdc/mpsl), plus this chip's real IRQ
 * vector routing for MPSL -- see docs/BLE_ROADMAP.md for how the four
 * glue functions were identified (linking a minimal test file against
 * the real vendored archives and reading `arm-none-eabi-nm -u`'s
 * output, not guessed) and how the IRQ vector assignments below were
 * confirmed against Nordic's own real reference integration source.
 *
 * NOT wired up to BLE yet. Nothing in this repo calls mpsl_init() or
 * sdc_init() yet -- this file makes those calls linkable and routes
 * their interrupts, but nothing enables those interrupts at the NVIC
 * yet (see the IRQ vector routing section below for why that's
 * intentional and safe).
 */
#include <mpsl_hwres.h>
#include <mpsl_hwres_ppi.h>
#include <mpsl.h>
#include <nrfx.h>
#include <helpers/nrfx_flag32_allocator.h>

/* ---- DPPI channel allocation ------------------------------------------
 *
 * mpsl_hwres_dppi_channel_alloc()'s contract is "allocate a channel on
 * THIS specific DPPIC instance" -- it does not need this project's own
 * cross-domain routing/connection logic (extern/nrfx/helpers/nrfx_gppi*),
 * which is a much larger subsystem (DPPI<->PPIB bridge routing across
 * power domains) this core doesn't otherwise use. A small per-instance
 * flag32 allocator is all MPSL's contract actually requires, so that's
 * what's implemented here instead of pulling in nrfx_gppi.
 *
 * Channel counts below are read directly from
 * extern/nrfx/bsp/stable/mdk/nrf54l/nrf54l15/nrf54l15_application_peripherals.h
 * (DPPICxx_CH_NUM_MAX), not assumed:
 *   DPPIC00: 8 channels, DPPIC10: 24, DPPIC20: 16, DPPIC30: 4.
 */
typedef struct
{
    NRF_DPPIC_Type * p_reg;
    nrfx_atomic_t    mask;
    bool             initialized;
} dppic_alloc_slot_t;

static dppic_alloc_slot_t s_dppic_slots[] = {
    { NRF_DPPIC00, 0, false }, /* 8 channels */
    { NRF_DPPIC10, 0, false }, /* 24 channels */
    { NRF_DPPIC20, 0, false }, /* 16 channels */
    { NRF_DPPIC30, 0, false }, /* 4 channels */
};

static uint32_t dppic_channel_count(NRF_DPPIC_Type * p_reg)
{
    if (p_reg == NRF_DPPIC00) return 8;
    if (p_reg == NRF_DPPIC10) return 24;
    if (p_reg == NRF_DPPIC20) return 16;
    if (p_reg == NRF_DPPIC30) return 4;
    return 0;
}

bool mpsl_hwres_dppi_channel_alloc(NRF_DPPIC_Type * p_dppic, uint8_t * p_dppi_ch)
{
    for (size_t i = 0; i < (sizeof(s_dppic_slots) / sizeof(s_dppic_slots[0])); i++)
    {
        if (s_dppic_slots[i].p_reg != p_dppic)
        {
            continue;
        }

        if (!s_dppic_slots[i].initialized)
        {
            uint32_t count = dppic_channel_count(p_dppic);
            nrfx_flag32_init(&s_dppic_slots[i].mask, (count >= 32) ? 0xFFFFFFFFUL : ((1UL << count) - 1UL));
            s_dppic_slots[i].initialized = true;
        }

        int channel = nrfx_flag32_alloc(&s_dppic_slots[i].mask);
        if (channel < 0)
        {
            return false;
        }

        *p_dppi_ch = (uint8_t)channel;
        return true;
    }

    /* p_dppic doesn't match any known nRF54L15 DPPIC instance. */
    return false;
}

/* ---- PPIB channel allocation -------------------------------------------
 *
 * Same reasoning as the DPPIC allocator above. Channel counts
 * (NTASKSEVENTS) are read directly from the same peripherals header,
 * not assumed: PPIB00/01/10/20: 8, PPIB11/21: 16, PPIB22/30: 4.
 */
typedef struct
{
    NRF_PPIB_Type * p_reg;
    nrfx_atomic_t   mask;
    bool            initialized;
} ppib_alloc_slot_t;

static ppib_alloc_slot_t s_ppib_slots[] = {
    { NRF_PPIB00, 0, false }, /* 8 channels */
    { NRF_PPIB01, 0, false }, /* 8 channels */
    { NRF_PPIB10, 0, false }, /* 8 channels */
    { NRF_PPIB11, 0, false }, /* 16 channels */
    { NRF_PPIB20, 0, false }, /* 8 channels */
    { NRF_PPIB21, 0, false }, /* 16 channels */
    { NRF_PPIB22, 0, false }, /* 4 channels */
    { NRF_PPIB30, 0, false }, /* 4 channels */
};

static uint32_t ppib_channel_count(NRF_PPIB_Type * p_reg)
{
    if (p_reg == NRF_PPIB00 || p_reg == NRF_PPIB01 ||
        p_reg == NRF_PPIB10 || p_reg == NRF_PPIB20)
    {
        return 8;
    }
    if (p_reg == NRF_PPIB11 || p_reg == NRF_PPIB21)
    {
        return 16;
    }
    if (p_reg == NRF_PPIB22 || p_reg == NRF_PPIB30)
    {
        return 4;
    }
    return 0;
}

bool mpsl_hwres_ppib_channel_alloc(NRF_PPIB_Type * p_ppib, uint8_t * p_ppib_ch)
{
    for (size_t i = 0; i < (sizeof(s_ppib_slots) / sizeof(s_ppib_slots[0])); i++)
    {
        if (s_ppib_slots[i].p_reg != p_ppib)
        {
            continue;
        }

        if (!s_ppib_slots[i].initialized)
        {
            uint32_t count = ppib_channel_count(p_ppib);
            nrfx_flag32_init(&s_ppib_slots[i].mask, (count >= 32) ? 0xFFFFFFFFUL : ((1UL << count) - 1UL));
            s_ppib_slots[i].initialized = true;
        }

        int channel = nrfx_flag32_alloc(&s_ppib_slots[i].mask);
        if (channel < 0)
        {
            return false;
        }

        *p_ppib_ch = (uint8_t)channel;
        return true;
    }

    /* p_ppib doesn't match any known nRF54L15 PPIB instance. */
    return false;
}

/* ---- Low-latency acquire/release callbacks -----------------------------
 *
 * mpsl.h's own doc comment gives placing the NVM controller in a
 * low-latency mode as an *example* of what these do, not a mandated
 * mechanism -- MPSL calls these around time-critical radio work and
 * expects whatever the platform needs to reduce latency (if anything).
 *
 * These are deliberately NO-OP STUBS for now, not a real
 * implementation. The real mechanism on this chip is plausibly
 * `nrf_lrcconf_poweron_force_set()` (extern/nrfx/hal/nrf_lrcconf.h),
 * which can force a power domain to stay fully powered instead of
 * idling into a higher-latency state -- but which domain(s) SDC/MPSL
 * actually need forced active during radio activity on the nRF54L15 is
 * not something these vendored headers document, and guessing the wrong
 * domain mask here would be worse than a no-op: a no-op just costs
 * potential timing margin/power efficiency, while a wrong domain mask
 * could silently power down something SDC assumes is active. Needs the
 * real answer from Nordic's own MPSL/NCS reference integration before
 * this stops being a stub -- see docs/BLE_ROADMAP.md.
 *
 * MPSL's own doc note (mpsl.h) says it may skip intermediate release
 * calls for back-to-back time-critical events and only call release
 * after the last one -- callers must not assume acquire/release are
 * strictly paired. A future real implementation should use a counter
 * (acquire increments, release decrements, act only at the 0<->1
 * transition) rather than a boolean, for exactly that reason. Not
 * needed for a no-op stub, but noted here so the next implementation
 * doesn't have to rediscover it.
 */
void mpsl_low_latency_acquire_callback(void)
{
}

void mpsl_low_latency_release_callback(void)
{
}

/* ---- IRQ vector routing -------------------------------------------------
 *
 * MPSL exports MPSL_IRQ_RADIO_Handler()/MPSL_IRQ_RTC0_Handler()/
 * MPSL_IRQ_TIMER0_Handler()/MPSL_IRQ_CLOCK_Handler() as plain functions
 * (confirmed: `nm` on libmpsl.a shows them as defined/T symbols, not
 * undefined) -- the application just needs to route the real vector
 * table entries to them. Which physical vector each one is for the
 * nRF54L series is confirmed from Nordic's own real reference
 * integration (nrfconnect/sdk-nrf,
 * subsys/mpsl/init/include/mpsl_init_soc.h), not guessed:
 *
 *   MPSL_TIMER_IRQn = TIMER10_IRQn
 *   MPSL_RTC_IRQn   = GRTC_3_IRQn
 *   MPSL_RADIO_IRQn = RADIO_0_IRQn   -- NOT RADIO_1; confirmed from the
 *                                       same header that MPSL only ever
 *                                       touches RADIO_0. RADIO_1_IRQHandler
 *                                       is deliberately left alone here.
 *
 * The clock vector isn't wired in that same file -- found instead in
 * sdk-nrf's drivers/mpsl/clock_control/nrfx_clock_mpsl.c, whose
 * nrfx_clock_irq_handler() (the name nrfx's own
 * nrfx_irqs_nrf54l15_application.h maps to this chip's real
 * CLOCK_POWER_IRQHandler vector) does nothing but call
 * MPSL_IRQ_CLOCK_Handler().
 *
 * Confirmed from the same reference (subsys/mpsl/init/mpsl_init.c) that
 * each wrapper does *only* this one call -- no additional logic, no
 * nesting/locking (the nRF53-specific RTC pretick hook in Zephyr's
 * mpsl_rtc0_isr_wrapper doesn't apply to nRF54L/this chip).
 *
 * NOT setting NVIC priority or calling NVIC_EnableIRQ here. Confirmed
 * from the same reference that Zephyr's integration calls mpsl_init()
 * FIRST, and only afterward connects/enables these three IRQs
 * (TIMER0/RTC0/RADIO) at priority 0 (mpsl.h's MPSL_HIGH_IRQ_PRIORITY) --
 * i.e. this is the responsibility of whatever code actually calls
 * mpsl_init()/sdc_init(), which doesn't exist in this repo yet (see
 * docs/BLE_ROADMAP.md step 3).
 *
 * Gated behind ARDUINO_NRF54_MPSL_ENABLED, undefined by default --
 * unlike the DPPI/PPIB/low-latency functions above, these are NOT safe
 * to always compile in. Those four are only ever reached by a function
 * *call* (from mpsl_init()/sdc_init(), which nothing in this repo calls
 * yet), so --gc-sections strips them from any build that never calls
 * in. These four IRQHandler functions are different: the vector table
 * itself (`g_pfnVectors` in the vendored MDK startup .S, which is never
 * garbage-collected -- it's the binary's entry point) holds their
 * addresses as data the moment they're *defined*, regardless of whether
 * anything calls them. Confirmed this the hard way: defining them
 * unconditionally broke a plain Blink build with "undefined reference
 * to MPSL_IRQ_RADIO_Handler" and friends, because mpsl_glue.c lives in
 * cores/nrf54l/ and is compiled into every sketch, but Blink never
 * links extern/nordic_sdc/mpsl/lib/libmpsl.a (correctly -- it doesn't
 * use BLE). Since this project has no "opt-in library" home for BLE
 * code yet, gating behind a macro (rather than moving this file out of
 * cores/nrf54l/) is the pragmatic fix for now -- revisit once BLE has
 * an actual Arduino-facing library, at which point this IRQ routing
 * likely belongs there instead, compiled only when a sketch actually
 * uses it.
 */
#if defined(ARDUINO_NRF54_MPSL_ENABLED)

void RADIO_0_IRQHandler(void)
{
    MPSL_IRQ_RADIO_Handler();
}

void GRTC_3_IRQHandler(void)
{
    MPSL_IRQ_RTC0_Handler();
}

void TIMER10_IRQHandler(void)
{
    MPSL_IRQ_TIMER0_Handler();
}

void CLOCK_POWER_IRQHandler(void)
{
    MPSL_IRQ_CLOCK_Handler();
}

#endif /* ARDUINO_NRF54_MPSL_ENABLED */
