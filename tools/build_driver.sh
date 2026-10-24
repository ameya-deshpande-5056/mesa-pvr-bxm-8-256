#!/usr/bin/env bash
#
# build_driver.sh - End-to-end build script for PowerVR BXM-8-256 Vulkan driver.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
MESA_DIR="${ROOT_DIR}/research/mesa-src"
BUILD_DIR="${MESA_DIR}/build-android"
NATIVE_BIN_DIR="${ROOT_DIR}/tools/native-bin"
CROSS_FILE="${MESA_DIR}/android-aarch64.ini"

echo "=== PowerVR BXM-8-256 Driver Build & Package Pipeline ==="

# 1. Run host regression test first
echo "[*] Step 1: Running host regression test suite..."
gcc -Wall -Wextra -Werror -O2 "${ROOT_DIR}/tests/test_bridge_v25_mock.c" -o "${ROOT_DIR}/tests/test_bridge_v25_mock"
"${ROOT_DIR}/tests/test_bridge_v25_mock"
echo "[+] Host regression suite passed 100%!"

# 2. Check Android NDK
if [ -z "${ANDROID_NDK_ROOT:-}" ] && [ -z "${ANDROID_NDK_HOME:-}" ]; then
    if [ -d "/home/ameya/Android/Sdk/ndk/28.2.13676358" ]; then
        export ANDROID_NDK_ROOT="/home/ameya/Android/Sdk/ndk/28.2.13676358"
    else
        echo "[-] Error: ANDROID_NDK_ROOT or ANDROID_NDK_HOME must be set."
        exit 1
    fi
fi
NDK_PATH="${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME}}"
echo "[*] Using Android NDK: ${NDK_PATH}"

# 3. Ensure native host tools are available
mkdir -p "${NATIVE_BIN_DIR}"
export PATH="${NATIVE_BIN_DIR}:$PATH"

# 4. Generate cross file if needed
python3 "${ROOT_DIR}/tools/generate_cross_file.py" \
    --ndk "${NDK_PATH}" \
    --native-bin "${NATIVE_BIN_DIR}" \
    --out "${CROSS_FILE}" \
    --api 34

# 5. Configure Mesa build directory if not already configured
if [ ! -f "${BUILD_DIR}/build.ninja" ]; then
    echo "[*] Configuring Meson build for Android AArch64..."
    meson setup "${BUILD_DIR}" "${MESA_DIR}" \
        --cross-file "${CROSS_FILE}" \
        -Dbuildtype=release \
        -Dplatforms=android \
        -Dandroid-stub=true \
        -Dvulkan-drivers=imagination \
        -Dimagination-srv=true \
        -Dgallium-drivers= \
        -Degl=disabled \
        -Dgbm=disabled \
        -Dzstd=disabled \
        -Dallow-fallback-for=libdrm \
        -Dlibdrm:default_library=static \
        -Dmesa-clc=system \
        -Dprecomp-compiler=system
fi

# 6. Build the Vulkan driver
echo "[*] Step 2: Compiling libvulkan_powervr_mesa.so..."
ninja -C "${BUILD_DIR}" src/imagination/vulkan/libvulkan_powervr_mesa.so

# 7. Package the driver
echo "[*] Step 3: Packaging driver bundles..."
python3 "${ROOT_DIR}/tools/package_driver.py"

echo "=== Build and Packaging Complete! ==="
echo "Distribution packages are ready in: ${ROOT_DIR}/dist"

