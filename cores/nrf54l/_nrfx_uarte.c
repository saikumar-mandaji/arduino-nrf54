/* arduino-cli's core auto-discovery only scans files physically inside
 * cores/nrf54l/, but this driver lives in the vendored extern/nrfx
 * submodule. This thin wrapper (not a copy -- a textual #include of the
 * real vendored file) makes it visible to that scan without duplicating
 * or forking the submodule's source. The Makefile-based build compiles
 * the real path directly and does not use this wrapper. */
#include "../../extern/nrfx/drivers/src/nrfx_uarte.c"
