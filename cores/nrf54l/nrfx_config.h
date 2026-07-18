/**
 * @file nrfx_config.h
 * @brief Top-level nrfx configuration entry point for arduino-nrf54.
 *
 * nrfx itself does not ship this file -- it's the integrator's
 * responsibility (see nrfx's README "Integration notes"). This wrapper
 * exists only to satisfy the `#ifndef NRFX_CONFIG_H__` guard at the top of
 * Nordic's per-chip config template before including it; the actual option
 * values live in nrfx_config_nrf54l15_application.h, which is a vendored
 * copy of extern/nrfx/bsp/stable/templates/nrfx_config_nrf54l15_application.h
 * with only NRFX_UARTE_ENABLED and NRFX_GRTC_ENABLED flipped on.
 */
#ifndef NRFX_CONFIG_H__
#define NRFX_CONFIG_H__

#include "nrfx_config_nrf54l15_application.h"

#endif /* NRFX_CONFIG_H__ */
