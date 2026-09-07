/**
 * @file wiring_trng.c
 * @brief Implementation of the CRACEN-backed hardware RNG API declared
 * in wiring_trng.h.
 *
 * This chip has NO classic NRF_RNG peripheral -- confirmed directly by
 * compiling nrfx_rng.c for NRF54L15_XXAA, which fails outright
 * ("unknown type name 'NRF_RNG_Type'"): the only real random-number
 * hardware on this chip is CRACEN's TRNG sub-block, reached through
 * nrfx_cracen_entropy_get(). Same category of finding as this project's
 * earlier NVMC-vs-RRAMC discovery (see docs/VERIFICATION.md) -- the
 * classic nrfx driver simply doesn't apply to this chip.
 */
#include "wiring_trng.h"

#include <stdbool.h>
#include <string.h>

#include <nrfx_cracen.h>
#include <hal/nrf_cracen_rng.h>

static bool s_hwrng_ready = false;

bool hwrng_begin(void)
{
    if (s_hwrng_ready) {
        return true;
    }

    /* nrfx_cracen_init() returns 0 on success, -EALREADY if some other
     * caller (e.g. this core's own BLE bring-up in mpsl_glue.c) already
     * initialized the shared CRACEN peripheral -- both are "ready" from
     * this API's point of view. */
    int err = nrfx_cracen_init();
    if (err == 0 || err == -EALREADY) {
        s_hwrng_ready = true;
    }
    return s_hwrng_ready;
}

bool hwrng_get_bytes(uint8_t *buf, size_t size)
{
    if (!hwrng_begin()) {
        return false;
    }
    if (buf == NULL || size == 0) {
        return false;
    }

    size_t offset = 0;
    while (offset < size) {
        size_t chunk = size - offset;
        if (chunk > NRF_CRACEN_RNG_FIFO_SIZE) {
            chunk = NRF_CRACEN_RNG_FIFO_SIZE;
        }
        if (nrfx_cracen_entropy_get(buf + offset, chunk) != 0) {
            return false;
        }
        offset += chunk;
    }
    return true;
}

uint32_t hwrng_random32(void)
{
    uint32_t value = 0;
    if (!hwrng_get_bytes((uint8_t *)&value, sizeof(value))) {
        return 0;
    }
    return value;
}
