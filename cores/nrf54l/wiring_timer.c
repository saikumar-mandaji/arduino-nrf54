#include "wiring_timer.h"
#include <errno.h>
#include <nrfx_timer.h>

static nrfx_timer_t s_timer00 = NRFX_TIMER_INSTANCE(NRF_TIMER00);
static timer_callback_t s_user_callback = NULL;
static bool s_timer_began = false;

static void timer00_event_handler(nrf_timer_event_t event_type, void *p_context)
{
    (void)p_context;
    if (event_type == NRF_TIMER_EVENT_COMPARE0 && s_user_callback != NULL)
    {
        s_user_callback();
    }
}

/* nrfx_irqs_nrf54l15_application.h #defines this name to TIMER00_IRQHandler
 * -- defining it under this name is what actually populates the real
 * vector table slot, same convention as wiring_wdt.c's WDT31 handler. */
void nrfx_timer_00_irq_handler(void)
{
    nrfx_timer_irq_handler(&s_timer00);
}

static bool timer_lazy_init(void)
{
    if (s_timer_began)
    {
        return true;
    }

    /* 1 MHz tick: matches this core's GRTC-based micros() scale, so
     * period_us maps 1:1 onto ticks with no rounding surprises. */
    nrfx_timer_config_t config = NRFX_TIMER_DEFAULT_CONFIG(1000000);
    config.bit_width = NRF_TIMER_BIT_WIDTH_32;

    int err = nrfx_timer_init(&s_timer00, &config, timer00_event_handler);
    if (err != 0 && err != -EALREADY)
    {
        return false;
    }

    s_timer_began = true;
    return true;
}

bool timer_start_periodic_us(uint32_t period_us, timer_callback_t callback)
{
    if (period_us == 0 || callback == NULL)
    {
        return false;
    }
    if (!timer_lazy_init())
    {
        return false;
    }

    nrfx_timer_disable(&s_timer00);
    nrfx_timer_clear(&s_timer00);
    s_user_callback = callback;

    uint32_t ticks = nrfx_timer_us_to_ticks(&s_timer00, period_us);
    nrfx_timer_extended_compare(&s_timer00, NRF_TIMER_CC_CHANNEL0, ticks,
                                 NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, true);
    nrfx_timer_enable(&s_timer00);
    return true;
}

void timer_stop(void)
{
    if (s_timer_began)
    {
        nrfx_timer_disable(&s_timer00);
    }
    s_user_callback = NULL;
}
