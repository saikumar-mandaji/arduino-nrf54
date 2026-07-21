# Provenance

Vendored directly from Nordic Semiconductor's public `nrfconnect/sdk-nrfxlib`
GitHub repository (https://github.com/nrfconnect/sdk-nrfxlib), **not** copied
from any third-party fork or Arduino core.

- `softdevice_controller/lib/libsoftdevice_controller_multirole.a`
  fetched from `softdevice_controller/lib/nrf54l/hard-float/` at
  `sdk-nrfxlib` commit `21a0cb55e99e5a8a5efa6c31a9b36f37980a2e4a`
  ("softdevice_controller: rev c3c4107f0249dd15c3b604246bcfb477d388efef",
  2026-07-13). "multirole" variant chosen (supports both central and
  peripheral roles) over the smaller peripheral-only/central-only
  variants, since this core aims to expose both roles; the smaller
  variants remain an option later if flash footprint becomes a concern.
- `mpsl/lib/libmpsl.a` fetched from `mpsl/lib/nrf54l/hard-float/` at
  the same repository, `main` branch tip `ee21e4f531ab340702ae0a6ca73d1538102f8333`
  as of 2026-07-21 (MPSL's own lib directory did not have a more precise
  per-path commit checked separately).
- `hard-float` ABI variant was chosen (not `soft-float`/`softfp-float`)
  because this core's `Makefile` builds with `-mfloat-abi=hard
  -mfpu=fpv5-sp-d16` -- see `MCU_FLAGS` in the repo root `Makefile`.
  This must stay in sync: if the core's float ABI ever changes, these
  binaries need to be re-fetched from the matching variant directory.
- Public headers copied from `softdevice_controller/include/` and
  `mpsl/include/` at the same commits, unmodified.
- License: `LicenseRef-Nordic-5-Clause` (see `softdevice_controller/LICENSE.txt`
  and `mpsl/LICENSE.txt`, identical text) -- confirmed to permit binary-form
  redistribution (clause 2) as long as the copyright notice is kept and
  the result is only used with a Nordic Semiconductor IC. See
  `docs/BLE_ROADMAP.md` for how this was verified.
- The FEM (front-end module) libraries under `mpsl/fem/` were
  deliberately **not** vendored -- they're only needed when driving an
  external RF front-end/PA chip, which the nRF54L15-DK doesn't have.

## Why "multirole" and not central/peripheral-only

`sdk-nrfxlib` ships three SDC library variants: peripheral-only,
central-only, and multirole (both). Multirole was chosen for maximum
API surface while this core's BLE support is still being designed;
revisit if the multirole binary's flash footprint turns out to be a
problem for constrained parts (it should not be an issue on the
nRF54L15's 1.5 MB NVM used by this project's primary target board).
