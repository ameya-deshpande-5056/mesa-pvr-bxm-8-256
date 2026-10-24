# AGENT HANDOFF: PowerVR BXM-8-256 Mesa Vulkan Driver (`vulkan_pvr`)

> **FOR INCOMING AI AGENTS:** Read this document first! It gives you immediate context in under 1,500 tokens. For complete deep-dives, see [`PROJECT_STATUS.md`](PROJECT_STATUS.md).

---

## 1. Absolute Safety Directives (NON-NEGOTIABLE)

* **STRICT OFFLINE MODE:** NEVER run `adb`, `fastboot`, or device-side commands on the Redmi Note 14 5G.
* **ZERO LOW-LEVEL IOCTLS ON HARDWARE:** Previous unvalidated probes caused kernel panics during connection cleanup (`RemoveAndFreeStreamNode` Oops).
* **FROZEN PROBE:** [`tools/pvr_connect_probe.c`](tools/pvr_connect_probe.c) is permanently disabled with `#error`. Do not edit, unfreeze, or compile it.
* **WORK BOUNDARY:** All work is 100% host-bound (`x86_64` Linux host) using static analysis, binary disassembly, mock regression tests, and Android AArch64 cross-compilation.

---

## 2. Project Context & Objectives

* **Target Hardware:** Redmi Note 14 5G (`beryl`, MediaTek Dimensity 7025 / MT6855).
* **GPU:** Imagination PowerVR B-Series BXM-8-256.
* **Hardware BVNC:** `35.3.408.101` (Packed uint64: `0x0023000301980065`).
* **Kernel Driver:** Stock Imagination DDK `25.1@6715691` (`pvrsrvkm.ko`).
* **Primary End-Goal Applications:**
  * **Eden / Yuzu forks** (Nintendo Switch emulation on Android).
  * **Azahar / Citra forks** (Nintendo 3DS emulation on Android).
  * **Winlator / Gamehub** (PC gaming on Android via Box64 + Wine + DXVK).
* **Target Delivery Mechanism (COMPLETED):**
  * Rootless **`adrenotools` `.zip` driver package** ([`dist/vulkan_pvr_bxm8256_adrenotools.zip`](dist/vulkan_pvr_bxm8256_adrenotools.zip)) for Eden and Azahar custom driver menus.
  * **Dual HAL (`HMI`) + ICD (`vk_icdGetInstanceProcAddr`, `vk_icdNegotiateLoaderICDInterfaceVersion`) exports** ([`dist/vulkan_pvr_bxm8256_icd.zip`](dist/vulkan_pvr_bxm8256_icd.zip)) for Winlator/DXVK.
  * Packager tool: [`tools/package_driver.py`](tools/package_driver.py) validates exports, DT_NEEDED, and generates zip bundles automatically.

---

## 3. Current Git & Build State

* **Mesa Source Directory:** [`research/mesa-src`](research/mesa-src) (tracked as official Git submodule pointing to [`https://gitlab.freedesktop.org/ameya-deshpande-5056/mesa.git`](https://gitlab.freedesktop.org/ameya-deshpande-5056/mesa.git)).
* **Active Mesa Branch:** `bxm-ddk25-offline` (clean working tree, up to date with `myfork/bxm-ddk25-offline`).
* **HEAD Mesa Commit:** `f1a4d5929bcf` — *"imagination: Align compute workgroup and push constant limits with B-Series profile"*
* **Root Repository Commit:** `1a9986e` on `main` (tag `v0.1.0-alpha`).
  * Commit 1 ([`b60cf5fb70df`](research/mesa-src)): Added BVNC `35.3.408.101` profile (`bxm-8-256.h`).
  * Commit 2 ([`db29f27f10e8`](research/mesa-src)): Added DDK 25.1 bridge header (`pvr_srv_bridge_v25.h`), dual-stack dispatch, and B-Series 16-heap discovery.
  * Commit 3 ([`29856e95783c`](research/mesa-src)): Fixed PMR unref to Func 7 (`PMRUNREFPMR`), implemented DDK 25.1 PMR alloc for Func 8 (`PHYSMEMNEWRAMBACKEDPMR`), and updated verified hardware constants.
  * Commit 4 ([`c7d7cb3a4716`](research/mesa-src)): Exported Vulkan ICD entrypoints (`vk_icd*`) in `vulkan-android.sym` and statically bundled `libdrm.a` into `libvulkan_powervr_mesa.so` for zero non-Bionic dependencies.
  * Commit 5 ([`4dc1162ef527`](research/mesa-src)): Increased push constants to 256B, expanded compute workgroups up to 512 invocations (`ERN35421`), and fixed off-by-one assertion in `pvr_arch_cmd_buffer.c:6425`.

---

## 4. Key File Map

| Purpose | File Path |
| :--- | :--- |
| **Comprehensive Status** | [`PROJECT_STATUS.md`](PROJECT_STATUS.md) |
| **Packaging & Verification Tool** | [`tools/package_driver.py`](tools/package_driver.py) |
| **Adrenotools Zip (Eden / Azahar)** | [`dist/vulkan_pvr_bxm8256_adrenotools.zip`](dist/vulkan_pvr_bxm8256_adrenotools.zip) |
| **Standalone ICD Zip (Winlator / Gamehub)** | [`dist/vulkan_pvr_bxm8256_icd.zip`](dist/vulkan_pvr_bxm8256_icd.zip) |
| **Cross-Compiled Binary (12 MB ELF64)** | [`research/mesa-src/build-android/src/imagination/vulkan/libvulkan_powervr_mesa.so`](research/mesa-src/build-android/src/imagination/vulkan/libvulkan_powervr_mesa.so) |
| **Verified DDK 25.1 Header** | [`research/mesa-src/src/imagination/vulkan/winsys/pvrsrvkm/pvr_srv_bridge_v25.h`](research/mesa-src/src/imagination/vulkan/winsys/pvrsrvkm/pvr_srv_bridge_v25.h) |
| **BXM-8-256 Hardware Profile** | [`research/mesa-src/src/imagination/common/device_info/bxm-8-256.h`](research/mesa-src/src/imagination/common/device_info/bxm-8-256.h) |
| **Host Regression Harness** | [`tests/test_bridge_v25_mock.c`](tests/test_bridge_v25_mock.c) & binary [`tests/test_bridge_v25_mock`](tests/test_bridge_v25_mock) |
| **Vendor Device Properties Dump** | [`device-info/vkjson.json`](device-info/vkjson.json) |

---

## 5. Standard Test & Build Commands

### Run Host Regression Test (100% Offline)
```bash
gcc -Wall -Wextra -Werror -O2 /home/ameya/bxm-research/tests/test_bridge_v25_mock.c -o /home/ameya/bxm-research/tests/test_bridge_v25_mock
/home/ameya/bxm-research/tests/test_bridge_v25_mock
```

### Build Android AArch64 Driver Binary & Package Zips
```bash
export PATH="/home/ameya/bxm-research/tools/native-bin:$PATH"
ninja -C /home/ameya/bxm-research/research/mesa-src/build-android src/imagination/vulkan/libvulkan_powervr_mesa.so
python3 /home/ameya/bxm-research/tools/package_driver.py
```

---

## 6. Milestone Roadmap & Next Tasks

* **Milestones 1–10, 12 [COMPLETED & VERIFIED]:**
  * Forensic crash audit, DDK 25.1 bridge header, B-Series heap topology, PMR alloc/unref defect resolution, strengthened host regression harness, Android NDK r28 cross-compilation, Dual HAL+ICD export model, Adrenotools & Standalone ICD zip packaging, and PCO compute workgroup & push constant alignment.
* **Milestone 11 [NEXT / PLANNED]:**
  * **DRM-Shim / Mock Device Pipeline Verification:** Intercept `/dev/dri/renderD128` via `powervr_noop_drm_shim` or pvrsrvkm mock harness on Linux host to test command stream building (`pvr_cmd_buffer.c`) and pipeline compilation offline.
* **Milestones 13+ [FUTURE / STRICTLY BLOCKED]:**
  * Physical hardware validation remains suspended until offline DRM-shim and pipeline tests are completed.
