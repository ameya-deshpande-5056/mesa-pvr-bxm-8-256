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
* **Primary End-Goal Applications & Target Performance:**
  * **Realistic Target:** Stable **720p 30 FPS handheld gaming** for 2D indies (*Celeste*, *Hollow Knight*, *Dead Cells*, *Undertale*), retro ports (*Pokémon* GBA/NDS classic ports, *Castlevania*), and lightweight JRPGs in **Eden / Yuzu / Azahar / Winlator**.
  * **Hardware Reality:** The Dimensity 7025 (2× Cortex-A78 @ 2.5 GHz, ~260 GFLOPS GPU) has ~3.5× higher CPU IPC and higher GPU compute than the original Nintendo Switch Tegra X1 in handheld mode (~196 GFLOPS). The hardware is not the bottleneck.
  * **Why Games Crashed on Stock Driver:** Stock MediaTek `libusc.so` compiler threw `SIGSEGV` segmentation faults when compiling NVN-to-SPIR-V translated shaders; stock DDK clamped push constants and had ASTC format view mismatches. Mesa's standard-compliant NIR compiler fixes these compiler crashes.
* **Target Delivery Mechanism (COMPLETED):**
  * Rootless **`adrenotools` `.zip` driver package** ([`dist/vulkan_pvr_bxm8256_adrenotools.zip`](dist/vulkan_pvr_bxm8256_adrenotools.zip)) for Eden and Azahar custom driver menus.
  * **Dual HAL (`HMI`) + ICD (`vk_icdGetInstanceProcAddr`, `vk_icdNegotiateLoaderICDInterfaceVersion`) exports** ([`dist/vulkan_pvr_bxm8256_icd.zip`](dist/vulkan_pvr_bxm8256_icd.zip)) for Winlator/DXVK.
  * Packager tool: [`tools/package_driver.py`](tools/package_driver.py) validates exports, DT_NEEDED, and generates zip bundles automatically.

---

## 3. Current Git & Build State

* **Mesa Source Directory:** [`research/mesa-src`](research/mesa-src) (tracked as official Git submodule pointing to [`https://gitlab.freedesktop.org/ameya-deshpande-5056/mesa.git`](https://gitlab.freedesktop.org/ameya-deshpande-5056/mesa.git)).
* **Active Mesa Branch:** `bxm-ddk25-offline` (clean working tree, up to date with `myfork/bxm-ddk25-offline`).
* **HEAD Mesa Commit:** `f1a4d5929bcf` — *"imagination: Align compute workgroup and push constant limits with B-Series profile"*
* **Root Repository Commit:** `779672b` on `main` (tag `v0.1.0-alpha`).
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
  * Forensic crash audit, DDK 25.1 bridge header, B-Series heap topology, PMR alloc/unref defect resolution, strengthened host regression harness, Android NDK r28 cross-compilation, Dual HAL+ICD export model, Adrenotools & Standalone ICD zip packaging, and PCO compute workgroup (512 inv) & push constant (256B) alignment.
* **Milestone 11 [NEXT / IMMEDIATE]:**
  * **DRM-Shim / Mock Device Pipeline Verification:** Intercept `/dev/dri/renderD128` via `powervr_noop_drm_shim` or pvrsrvkm mock harness on Linux host to test command stream building (`pvr_cmd_buffer.c`) and pipeline compilation offline without touching hardware.
* **Driver Feature Expansion Roadmap [PLANNED / THINGS TO DO]:**
  * **M17 (Native ASTC Textures):** Populate `pvr_formats.c` format table for `VK_FORMAT_ASTC_*`. Hardware TPU supports ASTC; enables Switch (Eden) & 3DS (Azahar) textures.
  * **M18 (Depth Clamping):** Hook depth-clamp state bit into ISP command packets in `pvr_arch_cmd_buffer.c`.
  * **M19 (Descriptor Indexing / Bindless):** Implement NIR indirect descriptor load lowering to global memory (Turnip style) to support modern Winlator/DXVK games.
  * **M20 (Int64 / Int8 Math):** Hook `nir_lower_int64` and `nir_lower_bit_size` in PCO backend compiler.
  * **M21 (Multi-Viewport):** Expand `PVR_MAX_VIEWPORTS` and route PPP viewport state.
  * **M22 (Geometry & Tessellation):** Implement intermediate Parameter Buffer (PB) streaming passes in PCO (emulating GS/Tess on TBDR).
  * **M23 (BC Texture Decompression):** Hardware lacks BC units (`0.0`); implement compute shader decompression for desktop textures in Winlator.
* **Milestones 13–16: Physical Device Validation [STRICTLY SUSPENDED / GATED]:**
  * *Reason for Freeze:* Early probe (`pvr_connect_probe.c`) triggered a kernel Oops in `RemoveAndFreeStreamNode` during connection cleanup, rebooting the phone. Probe is locked with `#error`.
  * *Unfreeze Gate:* Requires 100% completion of M11 (offline DRM-shim mock) and explicit user authorization.
  * **M13 (Pre-Flight Safety Gate):** Battery > 50%, direct USB cable, background persistent logcat logging, dm-verity read-only check.
  * **M14 (Non-Invasive Deployment):** App-private sandbox execution via Adrenotools zip or `/data/local/tmp/mesa-test/`; zero system/vendor partition changes.
  * **M15 (Device Handshake & Memory Context):** Validate Bridge 1 (`SRVCORE_CONNECT` bit-5 `0x20`), BVNC `35.3.408.101` match, Bridge 6 (`DEVMEMINTCTXCREATE`), and clean teardown with zero panics.
  * **M16 (First Hardware Compute Execution):** 1-workgroup compute shader writing `42` to PMR buffer via `RGXKICKCDM` (Bridge 129) to prove physical silicon execution. Full specs in [`PROJECT_STATUS.md`](PROJECT_STATUS.md).
