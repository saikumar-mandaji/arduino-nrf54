@ See _nrfx_uarte.c for why this wrapper exists (same reasoning, for the
@ vendored per-chip startup assembly file). Safe to use C-preprocessor
@ #include here because recipe.S.o.pattern passes -x assembler-with-cpp.
#include "../../extern/nrfx/bsp/stable/mdk/nrf54l/nrf54l15/gcc_startup_nrf54l15_application.S"
