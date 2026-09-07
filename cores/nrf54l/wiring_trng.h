/**
 * @file wiring_trng.h
 * @brief Hardware true-random-number-generator API, backed by this
 * chip's real CRACEN TRNG (not a software PRNG).
 *
 * Deliberately NOT named random()/randomSeed() -- those are classic
 * Arduino API names for a *seeded, deterministic* software PRNG
 * (calling randomSeed() with the same seed twice reproduces the same
 * sequence). This chip's CRACEN TRNG has no concept of a seed at all --
 * every call pulls fresh entropy from a real hardware noise source, so
 * giving it the random()/randomSeed() names would silently change their
 * documented semantics for any sketch that assumes reproducibility.
 * Matches this project's existing convention of not overloading an
 * Arduino API name when the underlying behavior genuinely differs (see
 * EEPROM.commit()'s documented no-op status in libraries/EEPROM).
 */
#ifndef WIRING_TRNG_H
#define WIRING_TRNG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fill a buffer with real hardware entropy from the CRACEN TRNG.
 *
 * Initializes CRACEN on first call (idempotent -- see hwrng_begin()).
 * Internally chunks the request into NRF_CRACEN_RNG_FIFO_SIZE-sized
 * reads if @p size exceeds the peripheral's single-call limit, since
 * nrfx_cracen_entropy_get() rejects an over-large request outright
 * (-E2BIG) rather than partially filling the buffer.
 *
 * @param[out] buf  Buffer to fill.
 * @param[in]  size Number of bytes to generate.
 * @return true on success, false if CRACEN failed to initialize or a
 *         chunked nrfx_cracen_entropy_get() call reported an error.
 */
bool hwrng_get_bytes(uint8_t *buf, size_t size);

/**
 * @brief Return one 32-bit true-random value from the CRACEN TRNG.
 *
 * @return A hardware-random 32-bit value, or 0 if CRACEN failed to
 *         initialize (indistinguishable from a genuine result of 0 --
 *         use hwrng_get_bytes() directly if that matters to the caller).
 */
uint32_t hwrng_random32(void);

/**
 * @brief Explicitly initialize CRACEN for TRNG use.
 *
 * Optional: hwrng_get_bytes()/hwrng_random32() call this automatically
 * on first use. Exposed separately because nrfx_cracen_init() "assumes
 * exclusive access to the CRACEN TRNG and CryptoMaster" (per its own
 * doc comment) -- a sketch that also uses this core's BLE bring-up
 * (mpsl_glue.c, which calls nrfx_cracen_init() itself for the SDC's
 * random source) will see this return the driver's own -EALREADY
 * (harmless, already handled internally), not a new, separate
 * initialization -- CRACEN is a single shared peripheral instance.
 *
 * @return true if CRACEN is initialized (including already-initialized
 *         by another caller such as the BLE stack), false on a genuine
 *         initialization failure.
 */
bool hwrng_begin(void);

#ifdef __cplusplus
}
#endif

#endif /* WIRING_TRNG_H */
