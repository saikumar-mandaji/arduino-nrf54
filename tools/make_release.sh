#!/usr/bin/env bash
# Builds a self-contained Arduino Boards Manager release archive.
#
# Arduino IDE's Boards Manager does NOT run `git submodule update` -- it
# just downloads and unzips whatever package_index.json points at. So this
# script vendors (copies, not symlinks/submodule-references) only the
# actually-needed subset of extern/nrfx and extern/CMSIS_6 into a versioned
# archive: the full nrfx checkout is ~300MB because it bundles MDK headers
# for every Nordic chip family (nRF51/52/53/91/92 and every nRF54L
# variant); this core only targets the nRF54L15, so only that variant's
# MDK files are included, cutting the archive down to a few MB.
#
# Usage: tools/make_release.sh <version>
# Produces: dist/arduino-nrf54-<version>.zip

set -euo pipefail

VERSION="${1:?Usage: make_release.sh <version, e.g. 0.1.0>}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="$(mktemp -d)"
PKG_NAME="arduino-nrf54-${VERSION}"
PKG_DIR="${STAGE_DIR}/${PKG_NAME}"
DIST_DIR="${REPO_ROOT}/dist"

mkdir -p "${PKG_DIR}"

echo "==> Copying core package files"
cp "${REPO_ROOT}/boards.txt" "${PKG_DIR}/"
cp "${REPO_ROOT}/platform.txt" "${PKG_DIR}/"
cp "${REPO_ROOT}/LICENSE" "${PKG_DIR}/"
cp "${REPO_ROOT}/README.md" "${PKG_DIR}/"
cp -r "${REPO_ROOT}/cores" "${PKG_DIR}/"
cp -r "${REPO_ROOT}/variants" "${PKG_DIR}/"
cp -r "${REPO_ROOT}/examples" "${PKG_DIR}/"

echo "==> Vendoring pruned nrfx (nRF54L15 only, not every Nordic chip family)"
NRFX_SRC="${REPO_ROOT}/extern/nrfx"
NRFX_DST="${PKG_DIR}/extern/nrfx"
mkdir -p "${NRFX_DST}"
cp "${NRFX_SRC}/nrfx.h" "${NRFX_DST}/"
cp "${NRFX_SRC}/LICENSE" "${NRFX_DST}/" 2>/dev/null || true
cp -r "${NRFX_SRC}/drivers" "${NRFX_DST}/"
cp -r "${NRFX_SRC}/hal" "${NRFX_DST}/"
cp -r "${NRFX_SRC}/haly" "${NRFX_DST}/"
cp -r "${NRFX_SRC}/helpers" "${NRFX_DST}/"
cp -r "${NRFX_SRC}/lib" "${NRFX_DST}/"
cp -r "${NRFX_SRC}/templates" "${NRFX_DST}/"

mkdir -p "${NRFX_DST}/bsp/stable/mdk/nrf54l" "${NRFX_DST}/bsp/stable/mdk/common"
cp "${NRFX_SRC}/bsp/stable/nrfx_bsp.h" "${NRFX_DST}/bsp/stable/"
cp "${NRFX_SRC}/bsp/stable/nrfx_ext.h" "${NRFX_DST}/bsp/stable/"
# soc/ is referenced directly by nrfx_bsp.h -- found the same way as the
# nrf54l_erratas.h fix above (compile against a fresh extraction, fix the
# real error).
cp -r "${NRFX_SRC}/bsp/stable/soc" "${NRFX_DST}/bsp/stable/"
cp "${NRFX_SRC}/bsp/stable/mdk/nrf.h" "${NRFX_DST}/bsp/stable/mdk/"
cp "${NRFX_SRC}/bsp/stable/mdk/nrf_erratas.h" "${NRFX_DST}/bsp/stable/mdk/"
cp "${NRFX_SRC}/bsp/stable/mdk/nrf_peripherals.h" "${NRFX_DST}/bsp/stable/mdk/"
cp "${NRFX_SRC}/bsp/stable/mdk/nrf_mem.h" "${NRFX_DST}/bsp/stable/mdk/"
cp -r "${NRFX_SRC}/bsp/stable/mdk/nrf54l/nrf54l15" "${NRFX_DST}/bsp/stable/mdk/nrf54l/"
# nrf54l_erratas.h, system_nrf54l.{c,h}, system_nrf54l_approtect.h all
# live as siblings of nrf54l15/, not inside it -- found by actually
# compiling against a fresh extraction of this script's output and
# fixing each real "No such file or directory" error in turn, not
# guessed in advance. Copy every top-level file in this directory
# (deliberately not recursive -- the other chip-family subdirectories
# alongside nrf54l15/ are NOT wanted) rather than keep discovering
# individual missing files one compile error at a time.
find "${NRFX_SRC}/bsp/stable/mdk/nrf54l" -maxdepth 1 -type f -exec cp {} "${NRFX_DST}/bsp/stable/mdk/nrf54l/" \;
cp -r "${NRFX_SRC}/bsp/stable/mdk/common/." "${NRFX_DST}/bsp/stable/mdk/common/"

echo "==> Vendoring pruned CMSIS_6 (Core/Include only, not DSP/NN/etc)"
CMSIS_SRC="${REPO_ROOT}/extern/CMSIS_6"
CMSIS_DST="${PKG_DIR}/extern/CMSIS_6"
mkdir -p "${CMSIS_DST}/CMSIS/Core"
cp "${CMSIS_SRC}/LICENSE" "${CMSIS_DST}/" 2>/dev/null || true
cp -r "${CMSIS_SRC}/CMSIS/Core/Include" "${CMSIS_DST}/CMSIS/Core/"

echo "==> Building archive"
mkdir -p "${DIST_DIR}"
ARCHIVE="${DIST_DIR}/${PKG_NAME}.zip"
rm -f "${ARCHIVE}"
if command -v zip >/dev/null 2>&1; then
  (cd "${STAGE_DIR}" && zip -rq "${ARCHIVE}" "${PKG_NAME}")
elif command -v 7z >/dev/null 2>&1; then
  (cd "${STAGE_DIR}" && 7z a -tzip -bd "${ARCHIVE}" "${PKG_NAME}" >/dev/null)
elif [ -f "/c/Program Files/7-Zip/7z.exe" ]; then
  (cd "${STAGE_DIR}" && "/c/Program Files/7-Zip/7z.exe" a -tzip -bd "${ARCHIVE}" "${PKG_NAME}" >/dev/null)
else
  echo "No zip tool found (tried zip, 7z, and 7-Zip's default install path)." >&2
  exit 1
fi

SIZE=$(stat -c%s "${ARCHIVE}" 2>/dev/null || stat -f%z "${ARCHIVE}")
SHA256=$(sha256sum "${ARCHIVE}" | cut -d' ' -f1)

rm -rf "${STAGE_DIR}"

echo "==> Done"
echo "Archive: ${ARCHIVE}"
echo "Size:    ${SIZE} bytes"
echo "SHA256:  ${SHA256}"
