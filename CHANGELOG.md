# Changelog

All notable changes to the PowerVR BXM-8-256 Mesa Vulkan Driver (`vulkan_pvr`) project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.2.0-alpha] - 2026-11-12

### Milestone 17: Native ASTC LDR Textures (host-validated)

Switch (Eden) and 3DS (Azahar) titles ship their UI, sprites and framebuffers as ASTC. The driver now advertises and implements ASTC LDR sampling instead of falling back to ETC2 transcoding.

### Added

* **ASTC LDR format coverage (`pvr_arch_formats.c`):** 28 `FORMAT_COMPRESSED()` entries covering all 14 Vulkan ASTC block sizes in UNORM and sRGB, mapped to `ROGUE_TEXSTATE_FORMAT_COMPRESSED_ASTC_*` hardware indices 0-13. sRGB uses the same hardware index as UNORM because image-word0 carries the gamma bit.
* **ASTC border colours (`pvr_arch_border.c`):** 14 compressed border-colour table entries so `VK_BORDER_COLOR_*` samplers get a correctly packed colour through the `U8U8U8U8` simple format on devices with `tpu_border_colour_enhanced`.
* **ASTC LDR coverage gate (`test_astc_format_coverage.py`):** an offline check that keeps the Vulkan→pipe mapping, the format table, the border table, the generated texstate indices and the feature/profile bits consistent (156 checks).

### Changed

* **`textureCompressionASTC_LDR`** is now driven by the device feature bit (`PVR_HAS_FEATURE(dev_info, astc)`) instead of being hardcoded `false`. The BXM-8-256 profile already set `has_astc = true`; nothing was consuming it.

### Fixed

* **Restored a buildable driver tree.** Mesa commit `7f6bd1acab92` renamed shared compiler and Vulkan sources from `ROGUE_*` to `VOLCANIC_*` before a Volcanic per-arch implementation existed, leaving 368 unique compile errors (`VOLCANIC_MAX_INSTANCES_PER_TASK`, `VOLCANIC_NUM_TEXSTATE_DWORDS`, `VOLCANIC_TEXSTATE_IMAGE_WORD0_TEXFORMAT_MAX_SIZE`, missing `rogue_get_temps()`, incomplete `VOLCANIC_VDMCTRL_STREAM_LINK0`, ...). The affected files were restored to their pre-migration content; the added Volcanic scaffolding remains in-tree as reference but is not built, and the BXM profile keeps `.arch = PVR_DEVICE_ARCH_ROGUE` so dispatch targets the compiled per-arch code. The AArch64 driver links again (12,094,472 bytes) and both zips were rebuilt.

### Verification

| Check | Result |
| :--- | :--- |
| `ninja -C build-android src/imagination/vulkan/libvulkan_powervr_mesa.so` | Links, 12,094,472 bytes |
| `python3 test_astc_format_coverage.py` | 156 checks, 0 failures |
| `tests/test_bridge_v25_mock` (fresh build) | Passed |
| `tests/test_drm_shim_mock` (fresh build) | Passed |
| `python3 tools/package_driver.py` | Exports, `DT_NEEDED`, both zips rebuilt |

> Native ASTC sampling on the BXM TPU is **not** yet verified on silicon; this milestone is validated at format-table, feature-advertisement and build level only.

---

### Milestone 18: ISP Depth Clamping (host-validated)

Geometry crossing the near/far planes is now clamped to the depth range instead of being clipped, and `depthClamp` is advertised to applications.

### Added

* **`depthClamp` implementation (`pvr_arch_cmd_buffer.c`):** `TA_OUTPUT_SEL.isp_position_depth_clamp_z` is set from a new `pvr_depth_clamp_enabled()` predicate that resolves `VK_MESA_DEPTH_CLIP_ENABLE_*` together with the core `depthClampEnable` state. `TA_STATE_PPP_CTRL.clip_mode` is derived from the same predicate, so the "don't clip" and "clamp Z" bits cannot drift apart.
* **Depth clamp wiring gate (`test_depth_clamp_wiring.py`):** offline check over the feature bit, the vendor `vkjson` evidence (`depthClamp = 1.0` on this device), the predicate and both of its consumers, the dynamic-state plumbing, and the generated packet bit positions (19 checks, mutation-probed).

### Changed

* **Dynamic clamp/clip updates:** `MESA_VK_DYNAMIC_RS_DEPTH_CLIP_ENABLE` is now a PPP update trigger, and the dynamic-only emit path re-emits TA output selects as well as ISP control. Previously a `depthClampEnable` change without a pipeline rebind left the ISP clamp bit stale.
* **`clip_mode` selection** was rewritten in terms of `pvr_depth_clamp_enabled()`; hardware behaviour is unchanged for every previously handled case.
* **`depthClamp` feature flag** flipped to `true` in `pvr_physical_device.c`.

### Known limitations

* Not exercised on silicon.
* `VK_EXT_depth_clamp_zero_one` is advertised unconditionally. The ISP clamp range for `isp_position_depth_clamp_z` is undocumented (it may clamp to [0, 1] in clip space, which is what the extension specifies), so the advertisement is kept — the sibling Turnip driver also advertises it unconditionally — and needs on-silicon confirmation with a narrowed `minDepth`/`maxDepth` viewport.
* `TA_OUTPUT_SEL.tsp_unclamped_z_pres` is deliberately left at its pipeline-compile-time value: it is the fragment shader ABI for `pco_data.fs.uses.z`, not per-draw state, so toggling it per draw would desynchronise shader and packet.

### Verification

| Check | Result |
| :--- | :--- |
| `ninja -C build-android src/imagination/vulkan/libvulkan_powervr_mesa.so` | Links (only pre-existing `-Wenum-conversion` warnings in PDCS code) |
| `python3 test_depth_clamp_wiring.py` | 19 checks, 0 failures |
| Mutation probe (feature off / clamp bit orphaned) | Both states rejected, sources restored byte-identically |

---

### Milestone 21: Multi-Viewport (host-validated)

Applications can now bind more than one viewport per draw. The vendor driver already reports `maxViewports = 16.0` / `multiViewport = 1.0` on this device; Mesa was hardcoding a single viewport.

### Added

* **`has_multiviewport` device-feature bit (`pvr_device_info.h`):** the bit was already set in the BXM-8-256 profile but did not exist in `struct pvr_device_features`, so the tree did not compile. It is now declared and consumed.
* **Multi-viewport region clip (`pvr_arch_cmd_buffer.c`):** the ISP scissor and the TA `REGION_CLIP` are single rectangles, so with more than one viewport they are now derived from the union of every (`viewport[i]`, `scissor[min(i, scissor_count - 1)]`) overlap. Both are conservative early-cull regions, so a union never culls visible geometry. The previous `assert(viewport_count == 1)` / `assert(scissor_count == 1)` are gone; a leftover assert would have aborted debug builds and silently culled geometry in release builds.

### Changed

* **`.multiViewport`** is advertised from `PVR_HAS_FEATURE(dev_info, multiviewport)` instead of being hardcoded `false`.
* **`.maxViewports`** is `PVR_MAX_VIEWPORTS` (16) instead of `1`, matching the device capability.
* **`PVR_MAX_VIEWPORTS`** raised from 1 to 16 so the per-viewport state arrays in `pvr_cmd_buffer.h` are wide enough.

### Reverted

* **An in-progress descriptor-indexing (M19) change was removed.** It set `shader*ArrayNonUniformIndexing`, every `descriptorBinding*UpdateAfterBind`, `runtimeDescriptorArray`, the four `*NonUniformIndexingNative` flags and `maxUpdateAfterBindDescriptorsInAllPools` to their enabled values while leaving `descriptorIndexing = false` — an invalid Vulkan feature combination — and its `lower_load_vulkan_descriptor` "bindless" branch was a no-op whose `if`/`else` bodies were identical. Advertising descriptor indexing before the lowering exists would hand applications a feature the driver cannot execute. M19 stays planned.
* **A `PVR_DEVICE_ARCH_VOLCANIC` arch flip was removed** for the same reason recorded in M17: the per-arch Volcanic implementation does not exist yet, so the profile keeps `.arch = PVR_DEVICE_ARCH_ROGUE` and dispatch targets code that is actually compiled.

### Verification

| Check | Result |
| :--- | :--- |
| `ninja -C build-android src/imagination/vulkan/libvulkan_powervr_mesa.so` | Links, 6,986,008 bytes |
| `python3 test_multiviewport_wiring.py` | 9 checks, 0 failures (mutation-probed twice) |
| `python3 test_astc_format_coverage.py` | 156 checks, 0 failures |
| `python3 test_depth_clamp_wiring.py` | 19 checks, 0 failures |
| `python3 test_volcanic_mock.py` / `test_volcanic_header_validation.py` | 8/8, 10/10 |
| `tests/test_bridge_v25_mock` / `tests/test_drm_shim_mock` (fresh builds) | Passed |
| `python3 tools/package_driver.py` | Exports, `DT_NEEDED`, both zips rebuilt |

> Multi-viewport rendering is **not** yet exercised on silicon. In particular, whether the union region clip interacts correctly with per-primitive `gl_ViewportIndex` culling needs an on-device draw.

---

### CI test corrections (L1 / L2)

The two previously failing CI suites were failing because of wrong expectations, not because of wrong headers. Before changing anything, every checked-in header was regenerated with `gen_pack_header.py` from its volcanic XML: `cr.h`, `lls.h` and `ppp.h` came out **byte-identical** to the copies in the tree.

* **`test_volcanic_mock.py` (`meson_configured`, was 7/8):** it looked for `gen_pack_header.py` inside `csbgen/meson.build`, which only contains `subdir('rogue')` / `subdir('volcanic')`; the generator is referenced from the per-arch `rogue/meson.build` and `volcanic/meson.build`. The check now inspects the file that owns each piece of wiring (subdirs in `csbgen/meson.build`, generator in both per-arch files) and additionally asserts that `texstate.xml` is generated for the volcanic arch. Now **8/8**.
* **`test_volcanic_header_validation.py` (was 7/10):**
  * `cr.h` required `COMPILE_TIME_ASSERT`, which `gen_pack_header.py` never emits. Replaced with a real structural marker, `VOLCANIC_CR_CR_REG_WORD0`.
  * `lls.h` required `pack`/`unpack`/`length`/`header`, but `lls.xml` is a constants-only source (buffer sizes and alignments), so no packet functions can exist. It now checks the constants it really generates (`CDM_CONTEXT_RESUME_BUFFER_SIZE`, `TA_STATE_BUFFER_SIZE`, `USC_SHARED_REGS_BUFFER_SIZE`, `TA_STATE_BUFFER_ALIGNMENT`).
  * `ppp.h` required `VOLCANIC_PPP`, but `csbgen/volcanic/ppp.xml` declares `prefix="TA"`, so the generated symbols are `VOLCANIC_TA_*` and `VOLCANIC_PPP` can never appear. It now checks `VOLCANIC_TA_`, `VOLCANIC_TA_TA_REG_WORD0` and `VOLCANIC_TA_CLIP_MODE`.
  * Now **10/10**.

Both corrections were mutation-probed: deleting a real `lls.h` constant and rewriting the `ppp.h` prefix each produced exactly one failure, after which the headers were restored byte-identically.

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

