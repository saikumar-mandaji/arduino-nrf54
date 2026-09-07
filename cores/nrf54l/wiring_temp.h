/**
 * @file wiring_temp.h
 * @brief On-die temperature sensor, backed by nrfx_temp (blocking mode).
 *
 * Returns hundredths of a degree Celsius (e.g. 2575 == 25.75 C), matching
 * nrfx_temp_calculate()'s own documented scale exactly -- not converted
 * to float, so this works without pulling in soft-float printf support
 * on a build that otherwise avoids it.
 */
#ifndef WIRING_TEMP_H
#define WIRING_TEMP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the on-die temperature sensor (idempotent).
 * @return true if the TEMP peripheral is ready to measure.
 */
bool temp_begin(void);

/**
 * @brief Blocking-read the die temperature.
 *
 * Calls temp_begin() automatically on first use.
 *
 * @return Temperature in hundredths of a degree Celsius, or INT32_MIN if
 *         the TEMP peripheral failed to initialize or the measurement
 *         did not complete.
 */
int32_t temp_read_centi_celsius(void);

#ifdef __cplusplus
}
#endif

#endif /* WIRING_TEMP_H */
