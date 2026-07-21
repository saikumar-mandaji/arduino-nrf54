/**
 * @file mpsl_glue.c
 * @brief Application-side glue functions required to link Nordic's
 * vendored MPSL (extern/nordic_sdc/mpsl) -- see docs/BLE_ROADMAP.md for
 * how these four functions were identified: a minimal test file calling
 * mpsl_init()/sdc_init() was linked against the real vendored archives
 * and `arm-none-eabi-nm -u` was used to read off the exact undefined
 * symbols, rather than guessing from documentation alone.
 *
 * NOT wired up to BLE yet. Nothing in this repo calls mpsl_init() or
 * sdc_init() yet -- this file only makes those calls linkable. See the
 * "IRQ vector routing not done yet" note near the bottom for the real
 * blocker on finishing BLE integration.
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
