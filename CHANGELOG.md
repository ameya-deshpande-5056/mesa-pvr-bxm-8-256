# Changelog

All notable changes to the PowerVR BXM-8-256 Mesa Vulkan Driver (`vulkan_pvr`) project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.1.0-alpha] - 2026-10-24

### Initial Offline Alpha Release: BXM-8-256 Turnip-Style Vulkan Driver

This release establishes the first complete offline build and packaging pipeline for Mesa's open-source `vulkan_pvr` driver targeting the **Imagination PowerVR B-Series BXM-8-256 GPU** (BVNC `35.3.408.101`) on the Redmi Note 14 5G (`beryl`, MediaTek Dimensity 7025 / MT6855).

> [!CAUTION]
> **Safety Notice:** Physical on-device validation remains strictly suspended due to previous kernel panics during bridge connection cleanup on stock vendor kernels. This release is validated exclusively offline via host static assertions, disassembly verification, mock kernel regression suites, and Android cross-compilation.

---

### Added

* **Dual Loading Architecture (Adrenotools + Vulkan ICD):**
  * Exported `HMI` for Android HAL module loaders and `adrenotools` in-app custom driver managers.
  * Exported standard Khronos ICD entrypoints (`vk_icdGetInstanceProcAddr`, `vk_icdGetPhysicalDeviceProcAddr`, `vk_icdNegotiateLoaderICDInterfaceVersion`) for desktop/container environments (Winlator, Gamehub, Box64, Wine, DXVK).
* **Automated Package Bundling (`tools/package_driver.py`):**
  * Produces `vulkan_pvr_bxm8256_adrenotools.zip` with flat `meta.json` manifest for direct import in **Eden / Yuzu** and **Azahar / Citra**.
  * Produces `vulkan_pvr_bxm8256_icd.zip` and `powervr_icd.aarch64.json` manifest for direct deployment in **Winlator** and **Gamehub**.
* **Zero Non-Bionic Shared Dependencies:**
  * Statically linked `libdrm` subproject (`-Dlibdrm:default_library=static`), eliminating external `libdrm.so` requirements. The driver links exclusively against standard Android Bionic system libraries.
* **BVNC `35.3.408.101` Device Profile (`bxm-8-256.h`):**
  * Registered BXM-8-256 device info table in Mesa common device definitions.
  * Extracted exact hardware parameters from kernel `.rodata`: `num_clusters = 1`, `tile_size = 32x32`, `slc_cache_line_size_bits = 1024`, `fbcdc_algorithm = 4`, `device_id = 0x35010101`.
* **Authoritative DDK 25.1 Bridge Dispatch (`pvr_srv_bridge_v25.h`):**
  * Implemented packed C11 `_Static_assert` validated structures for DDK 25.1 kernel services.
  * Added dual-stack runtime bridge dispatch in `pvr_srv_bridge.c`.
* **B-Series 16-Heap Topology Support (`pvr_srv_memctx.c`):**
  * Bypasses deferred demand-paged `"General SVM"` (Heap 0).
  * Directs all dynamic allocations to the 128 GB non-deferred `"General"` heap (Heap 1) starting at `0x8000000000`.
  * Implemented fallback for absent `"TQ3DParameters"` heap.
* **Host Regression Test Harness (`tests/test_bridge_v25_mock.c`):**
  * Validates all 25+ C11 bridge structure layouts at runtime.
  * Implements dispatch traps preventing regressions to PMR-unexport (Func 1) or context-destroy (Func 10).
  * Enforces handle type safety, double-free detection, partial failure unwinding, and out-of-memory error translation.
* **PCO Compute Workgroup & Push Constant Alignment:**
  * Increased push constant capacity to 256 bytes (`PVR_MAX_PUSH_CONSTANTS_SIZE`).
  * Enabled compute workgroup sizes up to 512 invocations when `ERN35421` is supported.
  * Verified register pressure clamping in `pco_ra.c` to prevent USC slot allocation overflows.

### Fixed

* **PMR Unreference Function ID:** Realigned `PVR_SRV_BRIDGE_MM_PMRUNREFPMR_V25` from Func 1 (`PMRUNEXPORTPMR`) to authoritative Func 7 (`PMRUNREFPMR`).
* **PMR Allocation Bridge ABI:** Fixed collision where legacy DDK 1.17 allocated PMR via Func 10 (`DEVMEMINTCTXDESTROY` in DDK 25.1). Wired version branch to Func 8 (`PHYSMEMNEWRAMBACKEDPMR`) with 56-byte input and 20-byte packed output structures.
* **PMR Lifecycle Safety:** Added NULL pointer guards and handle clearing in `pvr_srv_free_pmr` and `buffer_release` to eliminate double-free vulnerabilities.
* **Compute Workgroup Padding Assertion:** Corrected off-by-one check in `pvr_arch_cmd_buffer.c:6425` from `<` to `<=` to allow dispatches with maximum compute workgroup sizes.

---

### Distribution Packages

| File | Target Emulator / Environment | Format |
| :--- | :--- | :--- |
| `vulkan_pvr_bxm8256_adrenotools.zip` | Eden (Switch), Azahar (3DS), Skyline | Adrenotools Custom Driver Zip (`meta.json` + `.so`) |
| `vulkan_pvr_bxm8256_icd.zip` | Winlator, Gamehub, Box64, Wine, DXVK | Vulkan ICD Bundle (`powervr_icd.aarch64.json` + `.so`) |
| `powervr_icd.aarch64.json` | Linux / Container ICD Loader | Khronos ICD Manifest |

