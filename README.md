# PowerVR BXM-8-256 Mesa Vulkan Driver (`vulkan_pvr`)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Android%20AArch64-green.svg)](https://developer.android.com)
[![GPU](https://img.shields.io/badge/GPU-PowerVR%20BXM--8--256-blue.svg)](https://www.imaginationtech.com)
[![BVNC](https://img.shields.io/badge/BVNC-35.3.408.101-purple.svg)](device-info/vkjson.json)
[![Build & Release](https://github.com/ameya/bxm-research/actions/workflows/build-and-release.yml/badge.svg)](https://github.com/ameya/bxm-research/actions)

Turnip-like open-source [Mesa](https://gitlab.freedesktop.org/mesa/mesa) Vulkan driver (`vulkan_pvr`) for the **Imagination Technologies PowerVR B-Series BXM-8-256 GPU** running on top of stock vendor kernel services (**Imagination DDK 25.1**).

Designed to enable gaming and emulation on Android without requiring root, kernel replacement, or firmware flashing:
* **Nintendo Switch Emulation:** [Eden](https://github.com/eden-emulator) / Yuzu forks via `adrenotools` custom driver loader.
* **Nintendo 3DS Emulation:** [Azahar](https://github.com/azahar-emu) / Citra forks via `adrenotools` custom driver loader.
* **PC Gaming on Android:** [Winlator](https://github.com/brunodev85/winlator) / [Gamehub](https://github.com/gamehub-app) via Box64 + Wine + DXVK using standard Vulkan ICD manifests.

---

## Target Hardware & System Profile

| Parameter | Value | Reference / Source |
| :--- | :--- | :--- |
| **Target Device** | Xiaomi Redmi Note 14 5G (Codename: `beryl`) | Device dumps / Fastboot |
| **System on Chip** | MediaTek Dimensity 7025 (`MT6855`) | `/proc/cpuinfo` |
| **GPU Architecture**| Imagination PowerVR B-Series BXM-8-256 | `VkPhysicalDeviceProperties` |
| **Hardware BVNC** | **`35.3.408.101`** (`0x0023000301980065`) | Kernel module lookup table |
| **Vendor Device ID**| `0x35010101` (Vendor ID: `0x1010`) | Extracted from `vulkan.mtk.so` |
| **Stock Kernel Stack**| Imagination DDK `25.1@6715691` (`pvrsrvkm.ko`) | Kernel disassembly |
| **Memory Topology** | B-Series 16-Heap Architecture (128 GB `"General"` Heap) | Runtime hardware discovery |
| **Driver Package** | `libvulkan_powervr_mesa.so` (12 MB ELF64) | Built with Android NDK r28 (API 34) |

---

## Features & Architectural Highlights

* **Dual-Loading Compatibility:**
  * **Android HAL Mode (`HMI`):** Supported by Android's `hw_get_module` and `adrenotools` driver injection in Nintendo emulators.
  * **Vulkan ICD Mode (`vk_icdGetInstanceProcAddr`):** Supported by desktop/container Vulkan loaders in Winlator, Gamehub, Box64, and DXVK.
* **Zero Non-Bionic Dependencies:** Statically links `libdrm.a`, ensuring that `libvulkan_powervr_mesa.so` has zero third-party dynamic library dependencies. It links strictly against standard Android system libraries (`libc`, `libm`, `libdl`, `libz`, `libhardware`, `liblog`, `libnativewindow`, `libsync`).
* **Authoritative DDK 25.1 Bridge ABI:** Reverse-engineered from `pvrsrvkm.ko` dispatch registration tables. Includes C11 static assertions (`pvr_srv_bridge_v25.h`) guaranteeing byte-exact structure alignments and function IDs.
* **B-Series 16-Heap Memory Mapping:** Bypasses deferred demand-paged `"General SVM"` (Heap 0), mapping active allocations into the 128 GB `"General"` heap (Heap 1) starting at `0x8000000000`.
* **Hardware-Aligned PCO Compiler Profile:**
  * Up to 512 compute invocations per workgroup (`ERN35421`).
  * 256-byte push constant capacity (`PVR_MAX_PUSH_CONSTANTS_SIZE`).
  * Register pressure clamping to prevent USC slot allocation overflows.

---

## Installation & Usage

### 1. In Nintendo Emulators (Eden / Azahar / Skyline)
1. Download **`vulkan_pvr_bxm8256_adrenotools.zip`** from [GitHub Releases](https://github.com/ameya/bxm-research/releases).
2. Open **Eden** or **Azahar**.
3. Go to **Settings** > **GPU Driver Manager** > **Install Driver**.
4. Select the downloaded `.zip` file (do **not** extract it).
5. Choose **PowerVR BXM-8-256 Mesa** in the driver list and launch your game.

### 2. In PC Gaming Containers (Winlator / Gamehub)
1. Download **`vulkan_pvr_bxm8256_icd.zip`** from [GitHub Releases](https://github.com/ameya/bxm-research/releases).
2. Extract the archive into your container environment (e.g. `/usr/local/lib/` or `/opt/mesa/`).
3. Place `powervr_icd.aarch64.json` into `/usr/share/vulkan/icd.d/`:
   ```bash
   cp powervr_icd.aarch64.json /usr/share/vulkan/icd.d/
   ```
4. Set the environment variable:
   ```bash
   export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/powervr_icd.aarch64.json
   ```

---

## Building from Source

### Prerequisites
* Linux host (`x86_64`, Ubuntu 22.04+ or Debian 12+)
* Python 3.10+, Meson 1.3+, Ninja 1.11+
* Android NDK (r27c or r28)
* Host compiler toolchain (`gcc`, `g++`, `bison`, `flex`, `glslangValidator`, `pkg-config`)

### Setup and Build

```bash
# 1. Clone this repository with submodules (includes Mesa fork)
git clone --recursive https://github.com/ameya-deshpande-5056/vulkan-pvr-bxm8256.git
cd vulkan-pvr-bxm8256

# 2. Set Android NDK path
export ANDROID_NDK_ROOT=/path/to/android-ndk-r28

# 3. Build native host helpers, cross-compile the driver, and run tests
./tools/build_driver.sh
```

Distribution packages will be generated in `dist/`:
* `dist/vulkan_pvr_bxm8256_adrenotools.zip`
* `dist/vulkan_pvr_bxm8256_icd.zip`

---

## Running Host Regression Tests (100% Offline)

To verify the DDK 25.1 bridge ABI, PMR dispatch traps, handle lifecycle safety, and OOM unwinding without hardware access:

```bash
gcc -Wall -Wextra -Werror -O2 tests/test_bridge_v25_mock.c -o tests/test_bridge_v25_mock
./tests/test_bridge_v25_mock
```

---

## Safety & Operational Boundary

> [!WARNING]
> **Safety Notice:** Direct physical hardware testing on the Redmi Note 14 5G is currently suspended. Previous unvalidated low-level probes against `/dev/dri/renderD128` caused kernel panics during transport layer stream cleanup (`RemoveAndFreeStreamNode` Oops). Probe [`tools/pvr_connect_probe.c`](tools/pvr_connect_probe.c) remains permanently locked with `#error`. All validation in this repository is static, mock-tested, and cross-compiled offline.

---

## Repository Map

* **`patches/`**: 5 clean, upstream-compatible Git patches modifying Mesa:
  * `0001`: BVNC `35.3.408.101` device profile registration.
  * `0002`: DDK 25.1 bridge ABI and B-Series 16-heap memory topology.
  * `0003`: PMR allocation (Func 8) and unreference (Func 7) ABI correction.
  * `0004`: Dual HAL + ICD symbol exports and static `libdrm` linkage.
  * `0005`: PCO compute workgroup (512 invocations) and push constant (256B) limits.
* **`tools/`**: Build automation, cross-file generators, packager scripts, and setup utilities.
* **`tests/`**: Host-side DDK 25.1 mock kernel regression suite.
* **`device-info/`**: Open device properties, `vkjson` capability dumps, and service dumps from physical hardware.
* **`crash/`**: Forensic kernel panic logs and callstack dissections.
* **`PROJECT_STATUS.md`**: Master technical documentation and comprehensive architectural knowledge base.
* **`AGENT_HANDOFF.md`**: Token-optimized quick-start guide for incoming AI agents.

---

## License

This project is licensed under the [MIT License](LICENSE).
Mesa is licensed under the MIT License and standard open-source licenses detailed in its repository.

