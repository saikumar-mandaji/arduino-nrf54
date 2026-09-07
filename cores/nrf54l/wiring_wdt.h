/**
 * @file wiring_wdt.h
 * @brief Hardware watchdog timer (WDT) wrapper, backed by nrfx_wdt.
 *
 * Real hardware limitation, not an oversight: once started, this chip's
 * WDT (like every classic nRF WDT) CANNOT be stopped or reconfigured by
 * software -- only a full chip reset clears it. There is deliberately
 * no wdt_disable() in this API; wdt_begin() is a one-way commitment for
 * the life of the running firmware, same as upstream AVR/SAMD Arduino
 * cores' own watchdog libraries document.
 */
#ifndef WIRING_WDT_H
#define WIRING_WDT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the watchdog with the given timeout.
 *
 * If wdt_feed() is not called at least once within @p timeout_ms, the
 * chip resets. Safe to call only once per boot -- a second call returns
 * false without changing the already-running timeout (nrfx_wdt has no
 * "reconfigure a running instance" path in this driver's blocking-init
 * usage; see nrfx_wdt_reconfigure() in extern/nrfx if that's ever
 * needed instead).
 *
 * @param[in] timeout_ms Watchdog reload value in milliseconds.
 * @return true if the watchdog was started, false if it was already
 *         running or initialization failed.
 */
bool wdt_begin(uint32_t timeout_ms);

/**
 * @brief Feed (kick) the watchdog to prevent a reset.
 *
 * No-op if wdt_begin() was never called or failed.
 */
void wdt_feed(void);

#ifdef __cplusplus
}
#endif

#endif /* WIRING_WDT_H */
