/**
 * @file wiring_timer.h
 * @brief General-purpose periodic hardware timer, backed by nrfx_timer on
 * TIMER00.
 *
 * This is deliberately separate from millis()/micros()/delay() (which run
 * on GRTC, see wiring_time.c) -- TIMER00 is otherwise unused by this core,
 * so a sketch that needs its own fixed-period interrupt (a metronome tick,
 * a software PID loop, a sample-rate clock) doesn't have to share or
 * fight over GRTC's channels with the core's own timekeeping.
 *
 * Only one periodic callback is supported at a time (single hardware
 * timer, single compare channel) -- a second timer_start_periodic_us()
 * call replaces the previous callback/period rather than stacking.
 */
#ifndef WIRING_TIMER_H
#define WIRING_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*timer_callback_t)(void);

/**
 * @brief Start (or restart) a periodic hardware-timer interrupt.
 *
 * Initializes TIMER00 on first call. Calling this again while already
 * running reconfigures the period and callback in place.
 *
 * @param[in] period_us Period between callback invocations, in
 *                       microseconds. Must fit the timer's 32-bit tick
 *                       range at 1 MHz (roughly up to ~71 minutes);
 *                       longer periods should use GRTC-based application
 *                       code instead of this API.
 * @param[in] callback  Called from interrupt context (TIMER00_IRQHandler)
 *                       on every period elapsed -- keep it short, same
 *                       rule as any other ISR on this core.
 * @return true if the timer was started, false on invalid arguments or
 *         initialization failure.
 */
bool timer_start_periodic_us(uint32_t period_us, timer_callback_t callback);

/**
 * @brief Stop the periodic timer. Safe to call even if never started.
 */
void timer_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* WIRING_TIMER_H */
