/**
 * @file mpsl_glue.c
 * @brief Application-side glue functions required to link Nordic's
 * vendored MPSL (extern/nordic_sdc/mpsl), this chip's real IRQ vector
 * routing for MPSL, and the actual mpsl_init()/sdc_init()/sdc_enable()
 * bring-up sequence -- see docs/BLE_ROADMAP.md for how each piece was
 * identified and confirmed against real sources, not guessed.
 *
 * Everything in this file is gated behind ARDUINO_NRF54_MPSL_ENABLED
 * (undefined by default) and is inert unless something defines that
 * macro AND calls nrf54_mpsl_ble_init(). Nothing in this repo does
 * either yet -- there is still no Arduino-facing BLE API, no HCI host,
 * and NO HARDWARE VERIFICATION of any of this. Compile/link-verified
 * only (see the verification notes near nrf54_mpsl_ble_init() below).
 */
#include <mpsl_hwres.h>
#include <mpsl_hwres_ppi.h>
#include <mpsl.h>
#include <nrfx.h>
#include <helpers/nrfx_flag32_allocator.h>

#if defined(ARDUINO_NRF54_MPSL_ENABLED)
#include <sdc.h>
#include <sdc_soc.h>
#include <nrfx_cracen.h>
#include <stdlib.h>
#include <stdint.h>
#endif

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

/* ---- Low-priority processing ------------------------------------------
 *
 * MPSL pends an application-chosen IRQ (`low_prio_irq`, passed to
 * mpsl_init() below) whenever mpsl_low_priority_process() needs to run.
 * Nordic's own reference (Zephyr's subsys/mpsl/init/mpsl_init.c) defers
 * the actual call out of interrupt context into a worker thread, since
 * mpsl_low_priority_process()'s own doc says it may take "at least a
 * few 100 ms" -- running that directly inside an ISR would stall this
 * bare-metal core's entire main loop (and anything at equal/lower NVIC
 * priority) for that whole duration, which this core has no RTOS
 * thread/work-queue to defer into like Zephyr does.
 *
 * The pattern here mirrors that without inventing a task system: the
 * ISR only sets a flag, and nrf54_mpsl_process() -- meant to be called
 * from a sketch's loop() once BLE is actually exposed to sketches --
 * does the real (slow) work outside interrupt context. This is a
 * minimal stand-in, not a finished design: whether this should instead
 * be called automatically from cores/nrf54l/main.cpp's `while (1) {
 * loop(); }` (so sketches don't have to remember to call it) is a real
 * open question for whenever this core gets an actual Arduino-facing
 * BLE API -- see docs/BLE_ROADMAP.md.
 *
 * The IRQ number used here (SWI00_IRQn = 28) matches Nordic's own
 * documented default for the nRF54L series (sdk-nrf's
 * subsys/mpsl/init/Kconfig: `default 28 if SOC_COMPATIBLE_NRF54LX #
 * SWI00`), not picked arbitrarily.
 */
static volatile bool s_mpsl_low_prio_pending = false;

void SWI00_IRQHandler(void)
{
    s_mpsl_low_prio_pending = true;
}

void nrf54_mpsl_process(void)
{
    if (s_mpsl_low_prio_pending)
    {
        s_mpsl_low_prio_pending = false;
        mpsl_low_priority_process();
    }
}

/* ---- mpsl_init()/sdc_init()/sdc_enable() bring-up sequence -------------
 *
 * Ordering confirmed from the same real reference (sdk-nrf's
 * mpsl_init.c): mpsl_init() is called FIRST; only afterward are
 * RADIO_0/GRTC_3/TIMER10 connected and enabled at NVIC priority 0
 * (mpsl.h's MPSL_HIGH_IRQ_PRIORITY); the low-prio IRQ (SWI00 here) is
 * enabled at a lower priority (Zephyr uses priority 4 -- matched here
 * for consistency, though the exact value isn't documented as required,
 * just "lower than 0"). CLOCK_POWER is also enabled at a low priority
 * per mpsl.h's own doc ("should be lower priority than priority 0").
 *
 * Clock config: passing NULL to mpsl_init() uses MPSL's own documented
 * default (RC oscillator, MPSL_RECOMMENDED_RC_CTIV/_TEMP_CTIV,
 * MPSL_DEFAULT_CLOCK_ACCURACY_PPM = 250 -- confirmed by reading
 * mpsl_clock.h directly). This matters concretely: sdc_init() itself
 * fails with -NRF_EPERM if "MPSL needs to be configured with a LFCLK
 * accuracy of 500 ppm or better" (sdc.h's own doc) -- 250 ppm clears
 * that bar, so NULL is a valid, not just convenient, choice here. Using
 * a board's real external 32kHz crystal instead (better accuracy/power)
 * is a real future improvement, not implemented -- it needs board-
 * specific crystal characteristics this core doesn't have a place to
 * configure yet.
 *
 * Entropy: sdc_enable() fails with -NRF_EPERM ("entropy source is not
 * configured") unless sdc_rand_source_register() was called first
 * (sdc.h's own doc on sdc_enable()). Wired to this chip's real CRACEN
 * hardware CSPRNG (extern/nrfx's nrfx_cracen driver,
 * nrfx_cracen_ctr_drbg_random_get() -- confirmed
 * NRF_CRACEN_HAS_CRYPTOMASTER is actually 1 for this chip by compiling
 * a check against the real vendored headers, not assumed), not a
 * placeholder -- this is real hardware entropy, matching the Bluetooth
 * Core Spec's security requirements sdc_soc.h's own doc references.
 *
 * Memory: sdc_enable() needs a caller-allocated, 8-byte-aligned buffer
 * whose required size is only known at runtime, from sdc_cfg_set()'s
 * return value (sdc.h's own documented pattern -- not a fixed constant
 * that could silently go stale across SDC versions).
 * SDC_DEFAULT_RESOURCE_CFG_TAG + SDC_CFG_TYPE_NONE with no prior config
 * calls queries the minimal baseline size (no links/features configured
 * beyond SDC's fixed overhead) -- deliberately not configuring a
 * specific number of central/peripheral links here, since that's a real
 * design decision that belongs with the future Arduino-facing BLE API
 * (docs/BLE_ROADMAP.md step 6), not something to guess as part of
 * bring-up plumbing. malloc() is used for the buffer (this core's
 * syscalls.c already implements _sbrk, so malloc is real/functional
 * here, not untested) -- checked for both allocation failure and 8-byte
 * alignment (newlib-nano's malloc is expected to already provide this,
 * but checked rather than assumed).
 *
 * NOT HARDWARE VERIFIED. Compile/link-verified only -- see the
 * "Verified" note in docs/BLE_ROADMAP.md. In particular,
 * mpsl_init()'s LFCLK startup, CRACEN's actual entropy quality, and
 * sdc_enable()'s real behavior all need confirming on a real
 * nRF54L15-DK over SWD, the same discipline used for every other
 * peripheral in this core (docs/VERIFICATION.md).
 */
static uint8_t * s_sdc_mem = NULL;

static void mpsl_assert_handler(const char * file, const uint32_t line)
{
    (void)file;
    (void)line;
    /* mpsl.h: MPSL disables all interrupts before calling this and
     * resets the chip if we return -- nothing meaningful to do here
     * yet (no guaranteed-initialized logging path at this point). */
}

static void sdc_fault_handler(const char * file, const uint32_t line)
{
    (void)file;
    (void)line;
}

static void sdc_rand_poll(uint8_t * p_buff, uint8_t length)
{
    /* Return value intentionally ignored: sdc_rand_source_t's rand_poll
     * has no error-reporting path (void return) -- this is what the
     * vendored API contract allows, not an oversight. */
    (void)nrfx_cracen_ctr_drbg_random_get(p_buff, length);
}

static void sdc_hci_available_callback(void)
{
    /* Fires when HCI data/an HCI event is ready to read via
     * sdc_hci_get() (sdc_enable()'s own doc). No HCI host exists in
     * this repo yet (docs/BLE_ROADMAP.md steps 4-6 are still open) --
     * this is a placeholder with nothing to hand the data to. */
}

bool nrf54_mpsl_ble_init(void)
{
    static bool s_ready = false;

    if (s_ready)
    {
        return true;
    }

    if (mpsl_init(NULL, SWI00_IRQn, mpsl_assert_handler) != 0)
    {
        return false;
    }

    NVIC_SetPriority(RADIO_0_IRQn, 0);
    NVIC_SetPriority(GRTC_3_IRQn, 0);
    NVIC_SetPriority(TIMER10_IRQn, 0);
    NVIC_EnableIRQ(RADIO_0_IRQn);
    NVIC_EnableIRQ(GRTC_3_IRQn);
    NVIC_EnableIRQ(TIMER10_IRQn);

    NVIC_SetPriority(CLOCK_POWER_IRQn, 4);
    NVIC_EnableIRQ(CLOCK_POWER_IRQn);

    NVIC_SetPriority(SWI00_IRQn, 4);
    NVIC_EnableIRQ(SWI00_IRQn);

    if (nrfx_cracen_init() != 0)
    {
        return false;
    }

    if (sdc_init(sdc_fault_handler) != 0)
    {
        return false;
    }

    sdc_rand_source_t rand_source = { .rand_poll = sdc_rand_poll };
    if (sdc_rand_source_register(&rand_source) != 0)
    {
        return false;
    }

    int32_t mem_size = sdc_cfg_set(SDC_DEFAULT_RESOURCE_CFG_TAG, SDC_CFG_TYPE_NONE, NULL);
    if (mem_size < 0)
    {
        return false;
    }

    s_sdc_mem = (uint8_t *)malloc((size_t)mem_size);
    if (s_sdc_mem == NULL || ((uintptr_t)s_sdc_mem % 8U) != 0U)
    {
        return false;
    }

    if (sdc_enable(sdc_hci_available_callback, s_sdc_mem) != 0)
    {
        return false;
    }

    s_ready = true;
    return true;
}

#endif /* ARDUINO_NRF54_MPSL_ENABLED */
