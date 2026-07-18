/**
 * @file nrfx_glue.h
 * @brief nrfx integration glue for arduino-nrf54 (bare-metal, no RTOS).
 *
 * Based on extern/nrfx/templates/nrfx_glue.h; filled in with a plain
 * CMSIS-Core (Cortex-M33) bare-metal implementation -- no RTOS underneath,
 * matching the rest of this core.
 */
#ifndef NRFX_GLUE_H__
#define NRFX_GLUE_H__

/* mdk/nrf.h is nrfx's device selector: gated on -DNRF54L15_XXAA (passed by
 * the build), it sets __FPU_PRESENT/__DSP_PRESENT/IRQn_Type etc. for this
 * chip *before* including core_cm33.h itself, in the right order. Including
 * core_cm33.h directly here (without going through mdk/nrf.h first) fails
 * with "compiler generates FPU/DSP instructions for a device without
 * FPU/DSP" and "unknown type name 'IRQn_Type'", since those macros/types
 * would not be defined yet. */
#include <mdk/nrf.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NDEBUG
#define NRFX_ASSERT(expression)
#else
#define NRFX_ASSERT(expression) \
    do { if (!(expression)) { __BKPT(0); } } while (0)
#endif

#define NRFX_STATIC_ASSERT(expression) _Static_assert(expression, "nrfx static assertion failed")

/* IRQ handling: plain CMSIS NVIC calls, no RTOS-aware wrapping needed. */
#define NRFX_IRQ_PRIORITY_SET(irq_number, priority) NVIC_SetPriority((IRQn_Type)(irq_number), (priority))
#define NRFX_IRQ_ENABLE(irq_number)                 NVIC_EnableIRQ((IRQn_Type)(irq_number))
#define NRFX_IRQ_IS_ENABLED(irq_number)              (NVIC->ISER[(uint32_t)(irq_number) >> 5] & (1UL << ((uint32_t)(irq_number) & 0x1FUL)))
#define NRFX_IRQ_DISABLE(irq_number)                NVIC_DisableIRQ((IRQn_Type)(irq_number))
#define NRFX_IRQ_PENDING_SET(irq_number)            NVIC_SetPendingIRQ((IRQn_Type)(irq_number))
#define NRFX_IRQ_PENDING_CLEAR(irq_number)          NVIC_ClearPendingIRQ((IRQn_Type)(irq_number))
#define NRFX_IRQ_IS_PENDING(irq_number)             NVIC_GetPendingIRQ((IRQn_Type)(irq_number))

/* Single-core, no-RTOS critical section. Implemented as real functions
 * (see nrfx_glue.c) using a nesting counter + saved PRIMASK, rather than a
 * pair of brace-matching macros -- a nesting counter is robust regardless
 * of how ENTER/EXIT calls are laid out relative to other braces, whereas a
 * macro pair that opens/closes a shared block is fragile if a caller ever
 * puts control flow between the two. */
void nrfx_glue_critical_section_enter(void);
void nrfx_glue_critical_section_exit(void);

#define NRFX_CRITICAL_SECTION_ENTER() nrfx_glue_critical_section_enter()
#define NRFX_CRITICAL_SECTION_EXIT()  nrfx_glue_critical_section_exit()

#define NRFX_COREDEP_DELAY_DWT_BASED 0
#include <lib/nrfx_coredep.h>
#define NRFX_DELAY_US(us_time) nrfx_coredep_delay_us(us_time)

#define nrfx_atomic_t uint32_t

#define NRFX_ATOMIC_FETCH_STORE(p_data, value) __atomic_exchange_n((p_data), (value), __ATOMIC_SEQ_CST)
#define NRFX_ATOMIC_FETCH_OR(p_data, value)    __atomic_fetch_or((p_data), (value), __ATOMIC_SEQ_CST)
#define NRFX_ATOMIC_FETCH_AND(p_data, value)   __atomic_fetch_and((p_data), (value), __ATOMIC_SEQ_CST)
#define NRFX_ATOMIC_FETCH_XOR(p_data, value)   __atomic_fetch_xor((p_data), (value), __ATOMIC_SEQ_CST)
#define NRFX_ATOMIC_FETCH_ADD(p_data, value)   __atomic_fetch_add((p_data), (value), __ATOMIC_SEQ_CST)
#define NRFX_ATOMIC_FETCH_SUB(p_data, value)   __atomic_fetch_sub((p_data), (value), __ATOMIC_SEQ_CST)
#define NRFX_ATOMIC_CAS(p_data, old_value, new_value) \
    __atomic_compare_exchange_n((p_data), &(old_value), (new_value), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

#define NRFX_CLZ(value) ((value) == 0 ? 32 : __builtin_clz(value))
#define NRFX_CTZ(value) ((value) == 0 ? 32 : __builtin_ctz(value))

#define NRFX_EVENT_READBACK_ENABLED 1

/* No data cache on Cortex-M33 in this configuration. */
#define NRFY_CACHE_WB(p_buffer, size)
#define NRFY_CACHE_INV(p_buffer, size)
#define NRFY_CACHE_WBINV(p_buffer, size)

/* Nothing reserved for use outside nrfx in this core. */
#define NRFX_DPPI_CHANNELS_USED   0
#define NRFX_DPPI_GROUPS_USED     0
#define NRFX_PPI_CHANNELS_USED    0
#define NRFX_PPI_GROUPS_USED      0
#define NRFX_GPIOTE_CHANNELS_USED 0
#define NRFX_EGUS_USED            0
#define NRFX_TIMERS_USED          0

#ifdef __cplusplus
}
#endif

#endif /* NRFX_GLUE_H__ */
