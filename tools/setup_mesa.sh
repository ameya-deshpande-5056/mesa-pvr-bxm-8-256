#!/usr/bin/env bash
#
# setup_mesa.sh - Clone upstream Mesa and apply BXM-8-256 DDK 25.1 driver patches.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
MESA_DIR="${ROOT_DIR}/research/mesa-src"
PATCHES_DIR="${ROOT_DIR}/patches"

BASE_COMMIT="fa7228e1a8ae" # docs: add sha256 checksum for mesa 25.0.0

echo "=== PowerVR BXM-8-256 Mesa Setup ==="

if [ -d "${MESA_DIR}" ]; then
    echo "[*] Existing Mesa directory found at: ${MESA_DIR}"
    cd "${MESA_DIR}"
    if git rev-parse --verify bxm-ddk25-offline >/dev/null 2>&1; then
        echo "[+] Branch bxm-ddk25-offline already exists in ${MESA_DIR}"
        exit 0
    fi
else
    echo "[*] Cloning upstream Mesa..."
    mkdir -p "${ROOT_DIR}/research"
    git clone https://gitlab.freedesktop.org/mesa/mesa.git "${MESA_DIR}"
    cd "${MESA_DIR}"
    git checkout "${BASE_COMMIT}"
fi

echo "[*] Creating branch bxm-ddk25-offline and applying patches..."
cd "${MESA_DIR}"
git checkout -B bxm-ddk25-offline "${BASE_COMMIT}"

for patch in "${PATCHES_DIR}"/*.patch; do
    echo "  -> Applying $(basename "${patch}")..."
    git am < "${patch}" || git apply "${patch}"
done

echo "[+] Successfully configured Mesa with BXM-8-256 patches at: ${MESA_DIR}"

