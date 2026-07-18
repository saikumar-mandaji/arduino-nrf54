/**
 * @file nrfx_glue.c
 * @brief Critical-section implementation for nrfx_glue.h.
 *
 * Nesting-safe: only the outermost ENTER call disables interrupts, and
 * only the outermost matching EXIT call restores the PRIMASK that was in
 * effect before the first ENTER. Not interrupt-safe against calls from an
 * ISR context racing with the main thread -- this core has no RTOS/preemption
 * of the main app besides interrupts themselves, and nrfx's own drivers do
 * not call these from within their own IRQ handlers, so a single
 * (non-atomic) nesting counter is sufficient here.
 */
#include "nrfx_glue.h"

static volatile uint8_t s_critical_nesting = 0;
static volatile uint32_t s_saved_primask = 0;

void nrfx_glue_critical_section_enter(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (s_critical_nesting == 0)
    {
        s_saved_primask = primask;
    }
    s_critical_nesting++;
}

void nrfx_glue_critical_section_exit(void)
{
    s_critical_nesting--;
    if (s_critical_nesting == 0 && !s_saved_primask)
    {
        __enable_irq();
    }
}
