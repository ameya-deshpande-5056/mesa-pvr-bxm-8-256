# Milestone Execution Log

This file is the living implementation journal for the BXM-8-256 Mesa Vulkan driver milestones. It records the decisions, investigation steps, validation commands, and outcome for each milestone as work progresses.

## Execution Discipline

Every milestone is tracked using the same structure:

- Objective
- Decision log
- Implementation steps
- Files observed or changed
- Validation command(s)
- Outcome
- Follow-up / next milestone

No milestone is considered complete without fresh, recorded verification output.

## Architecture Correction

This project’s working assumption is now explicitly corrected to the following:

- IMG-BXM-8-256 is a Volcanic-architecture GPU, not an older Rogue-class device.
- The Rogue-era code paths and naming in the Mesasource tree must be treated as legacy infrastructure or compatibility scaffolding, not as a direct description of the active BXM target architecture.
- Future milestone work should therefore be evaluated with a Volcanic-first lens, especially for hardware feature mappings, format support, and architecture-specific command-pack generation.
- The broader upstream Mesa effort referenced by the project is a 2027 Volcanic bring-up effort from Imagination; this does not mean the local codebase is already Volcanic-native, only that it is the correct target model for future architecture alignment.

This correction is important because it changes the risk model for milestones such as M17, M18, M19, and M22: we should validate each assumption against Volcanic semantics rather than reusing older Rogue assumptions without confirmation.

---

## Initial Audit: Milestones M1–M10 and M12

This pass is meant to separate three categories of work:

- facts that remain valid on the BXM-8-256 target,
- assumptions that are valid but need architecture-aware reinterpretation,
- assumptions that need deeper revalidation before they are treated as a trusted roadmap.

### Summary verdict

The overall result is: M1–M10 and M12 are not automatically invalid, but they are not yet fully architecture-validated under the Volcanic model. They should be treated as a strong historical implementation record, not as a final Volcanic truth source.

### Milestone-by-milestone classification

- M1: VALID AS PROJECT FACTS
  - The repository organization and vendor asset ingestion work remains a factual prerequisite.
  - This is a groundwork task and is not architecture-sensitive in the same way as later driver features.
  - Status: valid.

- M2: VALID AS SAFETY/ROOT-CAUSE FACTS
  - The forensic crash audit and the decision to freeze unsafe hardware probing remain consistent with the repo’s own safety record and the kernel Oops evidence in [PROJECT_STATUS.md](PROJECT_STATUS.md).
  - This milestone is not a generic architecture claim; it is a safety and root-cause investigation.
  - Status: valid.

- M3: VALID
  - Host toolchain setup and Android cross-build environment are real prerequisites and remain valid regardless of the external architecture label.
  - Status: valid.

- M4: VALID BUT RECHECK ARCHITECTURE LABELING
  - BVNC registration and device profile identification are valid as local project facts.
  - The caution is not the existence of the BXM-8-256 profile, but whether later code paths were interpreted through a Rogue lens instead of a Volcanic one.
  - Status: valid, but architecture wording should be revalidated.

- M5: VALID AS ABI EXTRACTION / DISPATCH FACTS
  - The DDK 25.1 bridge extraction and the dispatch table work are concrete, repository-backed engineering facts.
  - This milestone is still important even under Volcanic review because it defines the ABI contract the local driver is built around.
  - Status: valid as ABI fact, not as proof of full architecture parity.

- M6: VALID AS HARDWARE-TOPOLOGY FACTS
  - The B-Series 16-heap topology and heap-fallback logic are strongly grounded in the reverse-engineering records and are hardware-specific to the BXM target.
  - This does not automatically generalize to all Volcanic GPUs, but it is valid as a BXM-8-256 discovery record.
  - Status: valid for this target, with limited portability scope.

- M7: VALID AS ABI CORRECTION FACTS
  - The PMR alloc/unref correction is a concrete bug fix with direct regression evidence in [tests/test_bridge_v25_mock.c](tests/test_bridge_v25_mock.c).
  - This is one of the strongest and most defensible milestones in the project.
  - Status: valid.

- M8: VALID
  - The host-side mock harness and regression safety work is concrete and already passed under fresh validation.
  - This milestone remains a strong host-only gate even under a Volcanic review.
  - Status: valid.

- M9: VALID
  - Cross-compilation for Android AArch64 is a real build prerequisite and is not architecture-specific in the same way as the GPU feature assumptions.
  - Status: valid.

- M10: VALID AS COMPATIBILITY LAYER / PACKAGING FACTS
  - The dual HAL + standalone ICD packaging model is a strong real-world compatibility achievement for the target environment.
  - This milestone is still valid as a compatibility layer and packaging result, but it should be treated as environment compatibility work rather than proof that the underlying GPU architecture is already fully mapped.
  - Status: valid, but not sufficient as a Volcanic architecture proof.

- M12: NEEDS REINTERPRETATION, NOT BLANKET REWRITE
  - This milestone is likely the most affected by the Volcanic correction because it includes hardware feature caps, compiler limits, and backend limits.
  - The local changes are still real and the repo historically documents them as B-Series-aligned. However, the wording and assumptions should be reviewed under a Volcanic-first model before they are treated as final implementation truth.
  - Status: needs reinterpretation.

### Overall audit conclusion

The project’s early milestones are not “wrong” in the sense of being fabricated; they are a concrete record of how the project was built on this specific BXM-8-256 target. The issue is that the code and documentation use a mixture of:

- BXM-specific facts,
- Rogue-era framework assumptions,
- local compatibility workarounds,
- and optimistic future milestone assumptions.

The correct next move is not to erase the history, but to split the milestone log into:

1. hardware/ABI facts confirmed for BXM-8-256,
2. compatibility work confirmed for the current build environment,
3. Volcanic-architecture assumptions that still need revalidation.

This is the cleanest way to preserve the project’s engineering history while making the current architecture model honest.

### Deeper architecture review for the key milestones

#### M4: `35.3.408.101` device profile registration

Evidence from [research/mesa-src/src/imagination/common/device_info/bxm-8-256.h](research/mesa-src/src/imagination/common/device_info/bxm-8-256.h):

- the device profile is explicitly tagged as `.arch = PVR_DEVICE_ARCH_ROGUE`;
- the feature table includes `.has_roguexe = true` and other Rogue-era feature names;
- the profile is valid as a BXM-8-256 device profile, but it is not a Volcanic-native architecture declaration.

Conclusion: the milestone is valid as target-specific reverse-engineering, but it should be treated as a compatibility profile for a legacy Rogue-era driver stack, not as evidence that the architecture layer is already Volcanic-native.

#### M6: B-Series heap topology

Evidence from [PROJECT_STATUS.md](PROJECT_STATUS.md) and the reverse-engineering notes in the same document shows that the 16-heap topology was recovered and implemented for BXM-8-256, including the 128 GB General heap mapping and deferred heap bypass logic.

Conclusion: this is still valid as a BXM-8-256 topology fact. It is not invalidated by the Volcanic correction, but it is a target-specific fact and should not be mistaken for a general Volcanic hardware contract.

#### M12: workgroup limits, compiler limits, and backend assumptions

Evidence from [research/mesa-src/src/imagination/vulkan/pvr_physical_device.c](research/mesa-src/src/imagination/vulkan/pvr_physical_device.c):

- `textureCompressionASTC_LDR = false`, `depthClamp = false`, `multiViewport = false`, `shaderInt64 = false`, `shaderInt8 = false`, `descriptorIndexing = false`;
- the code still relies on Rogue-specific utilities such as `rogue_get_render_size_max()` and `ROGUE_*` macros from the hardware utilities layer.

This tells us that M12 is in a partial compatibility state: the project has BXM-specific limits and a working host-side ABI, but the codebase still structurally sits in the Rogue-era PowerVR driver stack.

Conclusion: M12 is not invalid, but it should be reinterpreted as “BXM compatibility tuning within a Rogue-origin driver stack,” not as the final proof of a Volcanic-native implementation.

#### M17 and later roadmap milestones

The later feature roadmap is especially sensitive to this architecture distinction because the actual device/format path is still clearly tied to Rogue-era assumptions in the codebase. The strongest symptom is visible in [research/mesa-src/src/imagination/README.md](research/mesa-src/src/imagination/README.md#L1-L7), which still describes the driver as starting with Rogue-based GPUs. That means the future milestones should be considered Volcanic-targeted goals that still require an explicit porting or revalidation layer before they can be treated as mature implementation instructions.

Conclusion: the roadmap is still useful as an engineering plan, but it should be revised as a Volcanic-targeted roadmap, not as a final continuation of a fully validated Rogue implementation history.

### Audit of later milestones: M17 through M23

#### M17: Native ASTC texture support

Status: needs reinterpretation before implementation.

Evidence:
- In [research/mesa-src/src/imagination/vulkan/pvr_physical_device.c](research/mesa-src/src/imagination/vulkan/pvr_physical_device.c#L262-L271), `textureCompressionASTC_LDR` is explicitly false.
- In the same codebase, the format table is still not clearly exposing a complete ASTC format table for the target path.

Interpretation:
- This is a valid compatibility goal for the target, but the implementation plan is still written through a legacy Rogue-oriented format model.
- The real work is not just adding enums; it is validating how this GPU distinguishes native ASTC support and whether the underlying hardware-parsing path is compatible with the new Volcanic model.

#### M18: ISP depth clamping

Status: needs reinterpretation.

Evidence:
- The feature flag is still false in [research/mesa-src/src/imagination/vulkan/pvr_physical_device.c](research/mesa-src/src/imagination/vulkan/pvr_physical_device.c#L259-L271).
- The codebase also still contains large amounts of Rogue-specific ISP and hardware utility naming, as seen in the broad search results under [research/mesa-src/src/imagination](research/mesa-src/src/imagination).

Interpretation:
- This is a valid feature gap and a realistic milestone for the target, but any direct porting from older Rogue state-machine logic must be treated as a revalidation task rather than a simple copy.

#### M19: descriptor indexing and bindless lowering

Status: high-risk rewrite candidate.

Evidence:
- `descriptorIndexing` remains false in [research/mesa-src/src/imagination/vulkan/pvr_physical_device.c](research/mesa-src/src/imagination/vulkan/pvr_physical_device.c#L279-L320).
- The project notes explicitly describe this as a NIR and backend lowering task, which is exactly the kind of work that depends on architecture-specific semantics and backend assumptions.

Interpretation:
- This milestone is conceptually sound, but it is one of the least safe assumptions to carry forward without a Volcanic-first review.
- It should be treated as a design-level task with revalidation, not as a straightforward continuation of older Rogue work.

#### M20: 64-bit integer and 8-bit storage lowering

Status: needs reinterpretation.

Evidence:
- `shaderInt64 = false`, `shaderInt8 = false`, and related storage features remain disabled in the device feature table in [research/mesa-src/src/imagination/vulkan/pvr_physical_device.c](research/mesa-src/src/imagination/vulkan/pvr_physical_device.c#L279-L320).

Interpretation:
- The milestone is realistic, but the exact lowering strategy must be validated against the actual target architecture rather than assumed to mirror legacy Rogue lowering conventions.

#### M21: multi-viewport

Status: valid feature goal, but small implementation detail not proof of architecture parity.

Evidence:
- `multiViewport = false` is still present in the feature table.

Interpretation:
- This is a reasonable target feature, and it is less architecture-sensitive than descriptor indexing or geometry emulation.
- It is a good candidate for a narrow implementation pass once the architecture revalidation is complete.

#### M22: geometry and tessellation emulation

Status: long-term rewrite candidate; not ready for direct continuation.

Evidence:
- The project itself documents this as an advanced long-term goal and it explicitly depends on pipeline emulation and PB streaming assumptions.
- The codebase still has extensive Rogue-era assumptions structurally embedded in the driver stack.

Interpretation:
- This milestone is a long-term roadmap item and should be treated as a separate design effort, not as a direct continuation of the earlier driver milestones.

#### M23: BC texture compute decompression

Status: valid compatibility goal, but not a core architecture milestone.

Evidence:
- `textureCompressionBC = false` remains in the feature table and the project notes explicitly describe the hardware as lacking BC support.

Interpretation:
- This is a practical compatibility target for Winlator and desktop-style texture streams, but it is more of an emulation/fallback strategy than proof of GPU architecture parity.

### Overall result for M17–M23

The later milestones remain useful as feature planning, but they should not be treated as a linear continuation of the project’s earlier validated work. They are best read as a Volcanic-target roadmap that requires explicit architecture revalidation before implementation is considered dependable.

### Final architecture disposition

The cleanest reading of the full project is:

- The local repository contains a real BXM-8-256 reverse-engineering and compatibility effort.
- The host-side ABI, PMR bridge, and mock lifecycle work are valid and independently verified.
- The codebase is still heavily structured around Rogue-era naming, macros, and device assumptions.
- The target must therefore be treated as a Volcanic-first bring-up effort, not as a completed Rogue implementation being simply extended.
- The correct project posture is to preserve the historical milestone record, but to mark architecture-sensitive milestones as revalidation tasks rather than as final implementation claims.

This is the main distinction between “project history” and “architecture truth.” The former is a source of working facts; the latter is what remains to be confirmed under the Volcanic model before the roadmap becomes implementation-complete.

---

## Milestone Order in Force

1. M11: DRM-Shim / Mock Device Pipeline Verification
2. M17: Native ASTC Texture Support
3. M18: ISP Depth Clamping
4. M20: Int64 / Int8 Math Lowering
5. M19: Descriptor Indexing
6. M21: Multi-Viewport
7. M23: BC Texture Compute Decompression
8. M22: Geometry & Tessellation Emulation

This order follows the project’s offline-only constraints and the compatibility priority of the BXM-8-256 driver stack.

---

## Execution Log

### Milestone M11 — DRM-Shim / Mock Device Pipeline Verification

#### Objective
Validate the offline host-side mock bridge pipeline and ensure the PowerVR DDK 25.1 ABI rejects wrong function dispatches and keeps the lifecycle cleanup consistent before any physical-device testing is attempted.

#### Decision log
- Decision: keep validation host-bound and offline.
- Rationale: the project explicitly forbids hardware probing and requires the driver to be validated in a mock environment before any physical validation is considered.
- Active implementation status: expanded the host-side render-node mock scaffold under [tests/test_drm_shim_mock.c](tests/test_drm_shim_mock.c) to exercise the realistic offline sequence that a Mesa DRM-shim path would require: open, connect, BO create, BO map, queued submits, safe close, and failure-cleanup validation after an invalid submit attempt.
- Decision: use the existing host-side bridge harness as the gate for M11 instead of creating a custom DRM-shim stack from scratch.
- Rationale: the repo already contains a strong mock bridge harness and this is the lowest-risk way to validate the live ABI contract and lifecycle safety.

#### Files observed
- [tests/test_bridge_v25_mock.c](tests/test_bridge_v25_mock.c)
- [AGENT_HANDOFF.md](AGENT_HANDOFF.md)
- [research/mesa-src/src/imagination/vulkan/winsys/pvrsrvkm/pvr_srv_bridge_v25.h](research/mesa-src/src/imagination/vulkan/winsys/pvrsrvkm/pvr_srv_bridge_v25.h)

#### Validation command
```bash
cd /home/ameya/bxm-research && gcc -Wall -Wextra -Werror -O2 tests/test_bridge_v25_mock.c -o tests/test_bridge_v25_mock && ./tests/test_bridge_v25_mock
```

#### Outcome
The mock ABI suite passed successfully with fresh output.

Evidence summary:
- ABI definitions validated successfully.
- PMR dispatch trap checks passed: wrong use of Func 1 and Func 10 was detected and rejected.
- Handle safety checks passed: type mismatch, double-free, and NULL guard behavior were validated.
- Partial-failure cleanup passed: zero leaked handles remained.
- End-to-end lifecycle flow succeeded: connect -> PMR allocation -> DMA-BUF export -> free -> disconnect.

The terminal output concluded with:

> >>> ALL STRENGTHENED REGRESSION & ABI TESTS PASSED PERFECTLY! <<<

#### Status
M11: complete as a validated host-only safety gate.

#### Rework kickoff
The Volcanic-first rework has now started with a concrete architecture audit utility: [tools/volcanic_rework_audit.py](tools/volcanic_rework_audit.py). Its purpose is to inventory Rogue drift across the Imagination tree before any milestone rework is treated as architecture-complete.

The initial audit is expected to highlight the exact files that still carry Rogue assumptions, especially the device profile, physical-device dispatch, and format table infrastructure. This gives a clean boundary between the proven host-side work and the architecture changes that still need to be reworked for a true Volcanic model.

#### Next action
Rework M12 and the older architecture-sensitive milestones by decoupling the driver profile and dispatch layer from Rogue assumptions before moving into M17: Native ASTC Texture Support.

---

### Milestone M17 — Native ASTC Texture Support

#### Objective
Enable `textureCompressionASTC_LDR` support by adding the missing ASTC format entries and physical-device feature flag wiring.

#### Decision log
- Decision: inspect the format tables and feature discovery before any code patch.
- Rationale: the likely root cause is not a complete ASTC backend implementation, but a missing feature definition and format registration in the Vulkan format table.
- Decision: keep the first pass narrow and review the actual format registration points instead of guessing.
- Rationale: a broad patch without confirming the format table layout would risk incorrect enum mappings or unsupported Vulkan surface states.

#### Files observed
- [research/mesa-src/src/imagination/vulkan/pvr_physical_device.c](research/mesa-src/src/imagination/vulkan/pvr_physical_device.c)
- [research/mesa-src/src/imagination/vulkan/pvr_formats.c](research/mesa-src/src/imagination/vulkan/pvr_formats.c)
- [research/mesa-src/src/imagination/vulkan/pvr_formats.h](research/mesa-src/src/imagination/vulkan/pvr_formats.h)

#### Pre-requisite: build recovery (recorded before M17 could be validated)

The Mesa tree at commit `032044c13213` did not compile. The *"Refactor code structure for improved readability and maintainability"* commit (`7f6bd1acab92`) mechanically renamed shared compiler and Vulkan sources from `ROGUE_*` / `rogue_hw_utils.h` to `VOLCANIC_*` / `volcanic_hw_utils.h` before a Volcanic per-arch implementation existed, which produced 368 unique compile errors (`VOLCANIC_MAX_INSTANCES_PER_TASK`, `VOLCANIC_NUM_TEXSTATE_DWORDS`, `VOLCANIC_TEXSTATE_IMAGE_WORD0_TEXFORMAT_MAX_SIZE`, missing `rogue_get_temps()` / `rogue_max_wg_temps()`, incomplete `VOLCANIC_VDMCTRL_STREAM_LINK0`, ...).

Decision: restore the 57 files that commit touched to their pre-migration content (`git checkout a2eee7644388 -- <files>`) rather than inventing Volcanic hardware constants that no evidence supports. The Volcanic scaffolding added alongside it (volcanic csbgen XML, `volcanic_hw_*.h`, `vulkan/volcanic/*.c`) is kept in the tree as reference material but is not wired into the build, and the BXM profile keeps `.arch = PVR_DEVICE_ARCH_ROGUE` so the driver dispatches to the per-arch implementation that is actually compiled and tested.

Result: `ninja -C build-android src/imagination/vulkan/libvulkan_powervr_mesa.so` links a 12,094,472-byte AArch64 driver again.

#### Investigation findings
- `textureCompressionASTC_LDR` was hardcoded `false` in the physical device feature table.
- The `has_astc` bit already existed in `struct pvr_device_features` and is `true` for the BXM-8-256 profile, but nothing consumed it.
- The compressed texstate enum already carries all 14 ASTC LDR block sizes at hardware indices 0-13, so no new packet definitions were required.
- The format registry is per-arch (`pvr_arch_formats.c`), not `pvr_formats.c`, and its `FORMAT_COMPRESSED()` entries only listed ETC2/EAC formats.

#### Implementation
1. `pvr_arch_formats.c`: 28 `FORMAT_COMPRESSED()` entries (14 block sizes × UNORM/SRGB) mapping `PIPE_FORMAT_ASTC_*` to `ROGUE_TEXSTATE_FORMAT_COMPRESSED_ASTC_*`. sRGB shares the UNORM hardware index because sRGB decoding is selected by the image-word0 gamma bit, exactly as for ETC2.
2. `pvr_arch_border.c`: 14 compressed border-colour entries, so ASTC samplers using `VK_BORDER_COLOR_*` get a packed colour through the `U8U8U8U8` simple format (BXM advertises `tpu_border_colour_enhanced`).
3. `pvr_physical_device.c`: `.textureCompressionASTC_LDR = PVR_HAS_FEATURE(dev_info, astc)`.
4. `test_astc_format_coverage.py`: new offline L2.5 gate that ties the five tables together (Vulkan→pipe mapping, format table, border table, texstate indices, feature flag + BXM profile).

#### Validation
```bash
ninja -C build-android src/imagination/vulkan/libvulkan_powervr_mesa.so
# -> links libvulkan_powervr_mesa.so (12,094,472 bytes)

python3 /home/ameya/bxm-research/test_astc_format_coverage.py
# -> 156 checks, 0 failures, "ALL ASTC (M17) COVERAGE CHECKS PASSED"

gcc -Wall -Wextra -Werror -O2 tests/test_bridge_v25_mock.c -o tests/test_bridge_v25_mock && ./tests/test_bridge_v25_mock
# -> ALL STRENGTHENED REGRESSION & ABI TESTS PASSED PERFECTLY

gcc -Wall -Wextra -O2 tests/test_drm_shim_mock.c -o tests/test_drm_shim_mock && ./tests/test_drm_shim_mock
# -> PASS: render-node lifecycle, submission, failure cleanup

python3 tools/package_driver.py
# -> both adrenotools and ICD zips rebuilt and re-validated
```

#### Outcome
M17: implemented and validated **host-only**. The ASTC format tables and feature flag are compiled into the AArch64 driver; actual ASTC sampling on the BXM TPU is not yet verified on silicon. Switch (Eden) and 3DS (Azahar) content can now create and sample ASTC LDR images instead of relying on ETC2 transcoding.

#### Next action
M18 (ISP depth clamping), keeping the same offline evidence discipline: patch, cross-compile, re-run the host gates, document the result.

---

### Milestone M18 — ISP Depth Clamping (`depthClamp`)

#### Objective
Advertise and implement `VkPhysicalDeviceFeatures::depthClamp` so that geometry crossing the near/far planes is clamped to the depth range instead of being clipped or producing out-of-range depth values.

#### Decision log
- Decision: check what the hardware exposes before writing new packets.
- Rationale: `TA_STATE_PPP_CTRL.clip_mode` already had a `NO_FRONT_OR_REAR` mode and `TA_OUTPUT_SEL` already had an `isp_position_depth_clamp_z` bit, so the packet definitions exist; the gap was that nothing set the ISP clamp bit and the feature was hardcoded off.
- Decision: derive the TA clip mode and the ISP clamp bit from one shared predicate.
- Rationale: the two bits are a contract. Clipping without the clamp bit discards geometry; clamping without the clamp bit lets an out-of-range Z reach the depth test. Deriving both from `pvr_depth_clamp_enabled()` makes it impossible to move one without the other.
- Decision: leave `TA_OUTPUT_SEL.tsp_unclamped_z_pres` untouched.
- Rationale: it is part of the fragment shader ABI (`pco_data.fs.uses.z`, decided at pipeline compile time by `pco_nir.c`/`pco_trans_nir.c`), not per-draw state. Toggling it per draw would desynchronise the shader from the packet.

#### Evidence for the capability
`device-info/vkjson.json` (vendor `vulkan.mtk.so` on this exact device) reports `depthClamp = 1.0`, so the silicon and the stock DDK support it; the feature was disabled in Mesa, not missing in hardware.

#### Implementation
1. `pvr_arch_cmd_buffer.c`: new `pvr_depth_clamp_enabled()` helper resolving `VK_MESA_DEPTH_CLIP_ENABLE_{FALSE,TRUE,NOT_CLAMP}` + core `depthClampEnable` into a single answer.
2. `pvr_arch_cmd_buffer.c`: `TA_OUTPUT_SEL.isp_position_depth_clamp_z` now follows that predicate (it was never set).
3. `pvr_arch_cmd_buffer.c`: `TA_STATE_PPP_CTRL.clip_mode` is derived from the same predicate (behaviour-preserving rewrite of the previous switch).
4. `pvr_arch_cmd_buffer.c`: `MESA_VK_DYNAMIC_RS_DEPTH_CLIP_ENABLE` added to the PPP update trigger, and the dynamic-only emit path now re-emits TA output selects as well as ISP control — otherwise a `depthClampEnable` change without a pipeline rebind left the ISP clamp bit stale.
5. `pvr_physical_device.c`: `.depthClamp = true`.
6. `test_depth_clamp_wiring.py`: offline L2.6 gate over the feature bit, the vendor evidence, the predicate and both of its consumers, the dynamic-state plumbing, and the generated packet bit positions.

#### Validation
```bash
ninja -C build-android src/imagination/vulkan/libvulkan_powervr_mesa.so
# -> links (only pre-existing -Wenum-conversion warnings in pdcs code)

python3 /home/ameya/bxm-research/test_depth_clamp_wiring.py
# -> 19 checks, 0 failures, "ALL DEPTH CLAMP (M18) WIRING CHECKS PASSED"
```

Mutation probe (to prove the gate is not vacuous): flipping `.depthClamp` back to `false` and orphaning `isp_position_depth_clamp_z` each produced exactly one failure; the sources were then restored byte-identically (`diff -q`) and the gate passed again.

#### Outcome
M18: implemented and validated **host-only**. The clamp path is compiled into the AArch64 driver. Two things are explicitly not yet true:
- Depth clamping has not been exercised on silicon.
- `VK_EXT_depth_clamp_zero_one` is advertised unconditionally, and that is deliberate for now. The exact clamp range of the ISP `isp_position_depth_clamp_z` bit is not documented: it may clamp to [0, 1] in clip space (which is exactly what the extension specifies) rather than to the viewport depth range. Turnip — the Adreno TBDR driver in this same tree — also advertises `depthClampZeroOne = true` unconditionally (`src/freedreno/vulkan/tu_device.cc:713`). Settling the question needs an on-silicon measurement with a narrowed `minDepth`/`maxDepth` viewport; changing the advertisement before then would be a guess in the opposite direction.

#### Next action
M20 (Int64 / Int8 lowering) per the milestone order in force, or M19 (descriptor indexing) if Winlator/DXVK compatibility is prioritised first.

---

### CI test corrections — L1 (`test_volcanic_mock.py`) and L2 (`test_volcanic_header_validation.py`)

#### Objective
Clear the two suites that were failing before M17/M18 work started, without weakening them.

#### Investigation
Each checked-in header under `src/imagination/include/hwdef/` was regenerated with `csbgen/gen_pack_header.py` from its `csbgen/volcanic/*.xml` source. `cr.h`, `lls.h` and `ppp.h` — the three that failed — came out byte-identical to the tree copies (`diff -q`), so the headers were correct and the assertions were wrong. (For the record, `cdm.h`, `pbestate.h`, `pds.h` and `texstate.h` are *not* byte-identical to current generator output; that is a separate, deliberately untouched question.)

#### Corrections
1. **L1 `meson_configured`:** asserted `gen_pack_header.py` inside `csbgen/meson.build`, which only holds `subdir('rogue')` / `subdir('volcanic')`. The generator is referenced from `csbgen/rogue/meson.build:21` and `csbgen/volcanic/meson.build:17`. The check now reads the three files and asserts each piece of wiring where it actually lives, plus that `texstate.xml` is generated for volcanic.
2. **L2 `cr.h`:** replaced the impossible `COMPILE_TIME_ASSERT` expectation (the generator emits no such token) with `VOLCANIC_CR_CR_REG_WORD0`.
3. **L2 `lls.h`:** `lls.xml` declares only buffer size/alignment constants, so `pack`/`unpack`/`length`/`header` cannot exist. Now checks `VOLCANIC_LLS_CDM_CONTEXT_RESUME_BUFFER_SIZE`, `VOLCANIC_LLS_TA_STATE_BUFFER_SIZE`, `VOLCANIC_LLS_USC_SHARED_REGS_BUFFER_SIZE`, `VOLCANIC_LLS_TA_STATE_BUFFER_ALIGNMENT`.
4. **L2 `ppp.h`:** `csbgen/volcanic/ppp.xml` declares `prefix="TA"`, so symbols are `VOLCANIC_TA_*`. Replaced `VOLCANIC_PPP` with `VOLCANIC_TA_`, `VOLCANIC_TA_TA_REG_WORD0`, `VOLCANIC_TA_CLIP_MODE`. A comment records the prefix contract so it is not "fixed" back later.

#### Validation
```bash
python3 test_volcanic_mock.py                 # -> 8/8 tests passed
python3 test_volcanic_header_validation.py    # -> 10/10 headers: all checks passed
```
Mutation probes: deleting `VOLCANIC_LLS_TA_STATE_BUFFER_SIZE` and rewriting the `ppp.h` prefix each produced exactly one failure (`9/10`), then the headers were restored byte-identically and the suite returned to 10/10.

#### Outcome
L1: 8/8, L2: 10/10, both non-vacuous. No product code changed; only the assertions.

---

### Milestone M23 (stage 1) — BC texture emulation decode kernel

#### Objective
Move BC (DXTC) texture support forward without advertising a feature the driver cannot execute. The device has no BC decompression unit (`textureCompressionBC = 0.0`), and unlike M19/M22 there is no vendor implementation to port, so BC must be decoded in software.

#### Decision log
- Decision: build and unit-test the decode kernel first, wire the image path second.
- Rationale: the kernel's output can be checked exactly on the build host, while the image plumbing cannot be checked at all without silicon. Landing the verifiable half first means the next stage starts from a known-good decoder instead of debugging a decoder through a game.
- Decision: decode on the CPU, not in a compute shader.
- Rationale: compute execution itself is still unvalidated on this driver (M16), and texture upload happens before the first frame — the worst possible place for an untested dispatch.
- Decision: BC6H decodes to RGBA16F rather than RGBA8.
- Rationale: BC6H is HDR; 8 bits per channel would clip the range the format exists to carry. BC4/BC5 stay RGBA8 because the util decoders already expand the single/double channels.
- Decision: **do not** advertise `textureCompressionBC` yet.
- Rationale: advertisement promises all BC formats, and a BC image created without decoding on upload renders garbage instead of failing cleanly. A wrong texture is harder to diagnose than a clean `VK_ERROR_FORMAT_NOT_SUPPORTED`.

#### Implementation
1. `src/imagination/vulkan/pvr_bc_emulation.h/.c`: format table for all 16 BC VkFormats (target format, block bytes, target bits per pixel), `pvr_bc_format_info()`, `pvr_bc_format_is_compressed()`, `pvr_bc_decoded_level_size()` and `pvr_bc_decode_level()` dispatching to the `util/format` decoders (dxt1/3/5 incl. sRGB variants, rgtc1/2, bptc unorm/srgb/float/ufloat).
2. `src/imagination/vulkan/meson.build`: `pvr_bc_emulation.c` added to the driver sources so it is compiled for AArch64 and cannot rot.
3. `tests/test_bc_decode.c` + `tests/build_test_bc_decode.sh`: host unit test linking the real `util/format` sources from the tree.

#### Validation
```bash
tests/build_test_bc_decode.sh && tests/test_bc_decode
# -> 30 passed, 0 failed
ninja -C build-android src/imagination/vulkan/libvulkan_powervr_mesa.so
# -> links, 7,002,392 bytes
```
The test is not vacuous: it asserts exact texels (a BC1 block with `c0 = 0xF800` must decode to 255,0,0,255 at every pixel; a BC3 block must keep alpha 255; a BC4 endpoint 255 must expand to R=255 with G=B=0), plus format mapping, decoded sizes and a partial 2x2 level.

#### Outcome
Stage 1 of M23 is done and verified: a decode kernel with real tests, compiled into the driver, and **not** advertised. Remaining for M23: emulated internal format at image create, decode at buffer→image copy, image-view translation from BC to the emulated format, UNORM/SRGB class separation, and coverage for the copy/blit paths not handled. The memory trade-off must also be decided (RGBA8 is 8x the footprint of BC1; ASTC would be 2x but needs a BC→ASTC encoder that Mesa does not ship).

---

### Milestone M20 — 64-bit integers (`shaderInt64`)

#### Objective
Advertise and implement `VkPhysicalDeviceFeatures::shaderInt64`. The vendor driver reports `shaderInt64 = 1.0` on this device, and the USC has no 64-bit ALU and no 64-bit memory instruction, so the feature must be emulated as 32-bit pairs.

#### Investigation
- `nir_options.lower_int64_options` was already `~0U`, and `nir_lower_int64` already ran in `pco_nir_opt()`. So the ALU half looked done.
- It was not: `nir_lower_int64` consults `shader->options->lower_int64_options`, and with the feature disabled nothing ever generated 64-bit NIR to lower. The call was live but unexercised.
- The memory half was genuinely missing: a 64-bit UBO/SSBO/global/push-constant/shared access would reach the backend as a 64-bit access, and `should_vectorize_mem_cb()` explicitly refuses `bit_size > 32`, which shows the intended model is ≤32-bit accesses.

#### Decision log
- Decision: lower 64-bit memory accesses with `nir_lower_mem_access_bit_sizes` rather than teaching the USC path to handle 64-bit accesses.
- Rationale: the hardware has no 64-bit load/store. Splitting is the same shape as ir3's approach and keeps the backend's existing 32-bit assumption intact.
- Decision: run the split **before** `nir_lower_int64`.
- Rationale: the ALU lowering produces pack/unpack pairs around the load; if the load is still 64-bit when that runs, the pair wraps a 64-bit access that must then be split anyway. Splitting first makes both halves 32-bit at the same point.
- Decision: copy ir3's alignment rules (8-bit for 1-byte alignment, 16-bit for 2-byte, otherwise ≤32-bit).
- Rationale: an unaligned access cannot be widened, and a 64-bit value at 8-byte alignment should become two 32-bit accesses, not eight 8-bit ones.
- Decision: keep 64-bit **atomics** off.
- Rationale: `shaderBufferInt64Atomics` needs native 64-bit atomics in memory; a pair of 32-bit atomics is not equivalent for concurrent access.

#### Implementation
1. `pco_nir.c`: `pco_mem_access_size_align()` callback — caps width at 32 bits, honours alignment, returns `num_components`/`bit_size`/`align`/`shift`.
2. `pco_nir.c`: `nir_lower_mem_access_bit_sizes` pass over UBO, SSBO, global, constant, push-constant and shared modes, placed directly before `nir_lower_int64`.
3. `pvr_physical_device.c`: `.shaderInt64 = true` with a comment naming both halves of the lowering.
4. `test_int64_lowering.py`: 21-check gate over the ALU options, the callback, the mode list, the pass ordering (inside `pco_nir_opt`, before `nir_lower_int64`), the advertisements that must stay false, and the vendor `shaderInt64` evidence.

#### Validation
```bash
ninja -C build-android src/imagination/vulkan/libvulkan_powervr_mesa.so   # links
python3 test_int64_lowering.py                                            # 21/21
```
Mutation probes: withdrawing `.shaderInt64` and removing the 32-bit cap each produced exactly one failure; the sources were restored byte-identically (`diff -q`) and the gate returned to 21/21.

#### Outcome
M20 (`shaderInt64`): implemented and validated **host-only**. Not compiled into a shader that was actually executed, and not run on silicon. The remaining half of M20 — `storageBuffer8BitAccess` / `shaderInt8`, which the vendor also reports as supported — is **not** done: PCO has no `nir_lower_bit_size` hook, so 8-bit ALU could reach a backend that only has 16/32-bit ops. It stays off.

---

### Milestone M21 — Multi-Viewport (`multiViewport`)

#### Objective
Advertise and implement `VkPhysicalDeviceFeatures::multiViewport` so a single draw can target up to 16 viewports, which the vendor driver already supports on this device.

#### Starting state
The milestone commit (`6109b1c`) added `.has_multiviewport = true` to the BXM-8-256 profile and raised `PVR_MAX_VIEWPORTS` to 16, but:
- `has_multiviewport` did not exist in `struct pvr_device_features`, so the tree did not compile;
- nothing consumed the bit: `.multiViewport` was still `false` and `.maxViewports` still `1U`;
- an uncommitted follow-up had deleted the feature bit to get the build green again, which broke the gate (5/8).

#### Decision log
- Decision: declare the feature bit rather than hardcode `multiViewport = true`.
- Rationale: the same driver builds for every PVR device; the capability is a device property and the BXM-8-256 profile already carries the evidence.
- Decision: express the scissor/region clip as the **union** of every viewport overlap instead of picking viewport 0 or dropping early culling.
- Rationale: the ISP scissor and the TA `REGION_CLIP` are single rectangles in the packet stream. Both are conservative early-cull regions, so a bounding union is correct by construction (it can only cull less), while viewport 0's rectangle would cull geometry belonging to the other viewports. Dropping culling entirely would work but throws away a large amount of bandwidth on a TBDR part.
- Decision: pair viewport *i* with scissor `min(i, scissor_count - 1)`.
- Rationale: that is the Vulkan rule when fewer scissors than viewports are set.
- Decision: keep `maxViewports = PVR_MAX_VIEWPORTS` symbolic rather than a bare `16U`.
- Rationale: one source of truth for the cap; the gate resolves the macro instead of requiring a magic number at the use site.

#### Implementation
1. `pvr_device_info.h`: `bool has_multiviewport : 1;` added in alphabetical order to `struct pvr_device_features`.
2. `pvr_physical_device.c`: `.multiViewport = PVR_HAS_FEATURE(dev_info, multiviewport)` and `.maxViewports = PVR_MAX_VIEWPORTS`.
3. `pvr_arch_cmd_buffer.c`: `pvr_setup_isp_depth_bias_scissor_state()` now loops over `dynamic_state->vp.viewport_count`, unions the non-empty `pvr_get_viewport_scissor_overlap()` rectangles, and derives the ISP scissor words plus region clip from that union. The two single-viewport asserts were replaced by range asserts (`1 .. PVR_MAX_VIEWPORTS` and `scissor_count >= 1`).
4. `test_multiviewport_wiring.py`: added a ninth check that no `viewport_count == 1` / `scissor_count == 1` assert survives and that the region-clip loop exists; the `maxViewports` check now resolves a symbolic value through its `#define`.

#### Validation
```bash
ninja -C build-android src/imagination/vulkan/libvulkan_powervr_mesa.so
# -> links, 6,986,008 bytes

python3 test_multiviewport_wiring.py
# -> 9/9 checks, "All M21 multi-viewport wiring checks pass"
```
Mutation probes (to prove the two new behaviours are actually covered): ungating `.multiViewport` and restoring `assert(viewport_count == 1)` each produced exactly one failure; both sources were then restored byte-identically (`diff -q`) and the gate returned to 9/9.

Full sweep re-run in the same session: `test_astc_format_coverage.py` 156/156, `test_depth_clamp_wiring.py` 19/19, `test_volcanic_mock.py` 8/8, `test_volcanic_header_validation.py` 10/10, `test_bridge_v25_mock` and `test_drm_shim_mock` passed from fresh builds, and `tools/package_driver.py` rebuilt both zips after validating exports and `DT_NEEDED`.

#### Outcome
M21: implemented and validated **host-only**. Remaining on-silicon question: whether the union region clip interacts correctly with per-primitive `gl_ViewportIndex` selection, i.e. that no primitive is culled because its own viewport rectangle is a strict subset of the union.

#### M19 attempt that was reverted
Before M21 was finished, an uncommitted descriptor-indexing change was found in the working tree:
- `pvr_physical_device.c` enabled every `shader*ArrayNonUniformIndexing`, `descriptorBinding*UpdateAfterBind`, `descriptorBindingUpdateUnusedWhilePending`, `descriptorBindingPartiallyBound`, `descriptorBindingVariableDescriptorCount` and `runtimeDescriptorArray` flag, plus the four `*NonUniformIndexingNative` properties and `maxUpdateAfterBindDescriptorsInAllPools`, while leaving `descriptorIndexing = false` — the spec forbids the sub-features when the umbrella feature is off, so the advertised set was invalid;
- `pco_nir_vk.c` grew an `if (desc_set == UINT32_MAX)` branch whose two bodies were byte-identical, so no bindless path existed;
- `pco_data.h` gained `descriptor_buffer` / `has_descriptor_buffer` fields that nothing reads;
- the profile was switched to `PVR_DEVICE_ARCH_VOLCANIC`, which the M17 entry in this log already records as unbuildable without the per-arch port.

All of it was reverted, and the supporting `test_descriptor_indexing.py` (which asserted exactly those strings) was deleted: it was a gate for code that does not exist, not for a feature. M19 remains planned, with its real requirement stated in `PROJECT_STATUS.md`: a lowering that can fetch a descriptor through a runtime array index, which the PDS static-binding model does not provide today.

---

## 2026-11-12 — M23 stages 2-3, device harness, M19 stage 1

### M23: the image path

- Create-time substitution: a BC `VkImageCreateInfo` is rewritten to the emulated format before `vk_image_create`, so extent, mip layout and texstate all describe what the image stores. With the feature off, BC create returns `VK_ERROR_FORMAT_NOT_SUPPORTED` (spec-required); `PVR_BC_EMULATION=1` reaches the path for bring-up.
- Views: created with the emulated format, and `pvr_bc_view_format_is_compatible()` rejects a view naming a format the image cannot serve (different block layout, or UNORM vs sRGB class). BC1 RGB/RGBA equivalence is implemented because Vulkan requires it.
- Upload: `vkCmdCopyBufferToImage` decodes each region into a linear staging buffer (the destination is twiddled, so only the transfer engine can write it) and lets the ordinary buffer→image path write it out. Every skip condition now names itself in a log line: non-emulated destination, unmapped source, non-color aspect, block-misaligned offset, `depth != 1`, misaligned row length/height, region past the buffer.
- Refused loudly instead of silently: `vkCmdBlitImage`, `vkCmdCopyImage`, `vkCmdCopyImageToBuffer`, `vkCmdClearColorImage` on a BC image.
- BC6H sizing: the bptc decoders write four 32-bit floats per texel, but the target was declared RGBA16F (64 bpp), so everything sized by `pvr_bc_decoded_level_size()` was half the needed size. Target is now RGBA32F (128 bpp). The old test asserted the wrong number, so it could not catch this; it now asserts 128 bpp and checks a canary past the end of a correctly sized buffer.
- Verification: `tests/test_bc_decode` 45/45 (mutation to 64 bpp → 3 failures, including the canary), `test_bc_image_path.py` 22/22 (mutations to the advertised flag and the view translation both fail), clean AArch64 link. Unverified: cache/ordering between the staging upload and the transfer command.

### CI gap closed

`tests/build_test_bc_decode.sh` used to require the Android cross-build's generated headers, so the BC test only ran on a machine that had built the driver. It now generates `u_format_gen.h`, `u_format_pack.h`, `u_format_table.c` and `format_srgb.c` with Mesa's own scripts when no build tree is available (checked with `build-android` renamed away), and the workflow builds and runs it.

### Device harness (M13/M14), host-side only

`tools/device/{preflight,step2_smoke,build_smoke}.sh`, `tools/smoke/vk_enum.c` and a README. Every script refuses to run unless `PVR_DEVICE_AUTHORIZED=1`, and the refusal path was verified to run no `adb` command. `vk_enum` is a read-only equivalent of `vulkaninfo --summary`: it creates an instance, enumerates physical devices and prints name/type/versions/heaps — no images, buffers, queues or submissions, so it cannot reach the teardown path that caused the kernel Oops. Built for aarch64 with NDK r28. **Nothing in this session was executed against the phone.**

### M19 stage 1 (forensics), first evidence

From `binaries/libusc.so`:

- Exactly one string mentions bindless: `In bindless mode the last gather4 source must be a channel select.` at VA `0x27148`.
- Exactly one cross-reference to it, at `0x216224` (`adrp x3, 0x27000` / `add x3, x3, #0x148`). The surrounding calls all pass two strings plus a numeric id to `0x684ac`, i.e. this is the vendor compiler's diagnostic helper, not bindless-specific code.
- That message establishes that bindless is a **compiler mode** with encoded ISA operand constraints, not a driver-side loop over PDS entries.
- ISA flag names present: `LDGATHER4`, `LDGATHER4_C`, `LDGATHER4_PO`, `LDGATHER4_C_PO`, `GATHER4_PRECISION_WORKAROUND`, `RANGE_CHECK_NULL_DESCRIPTOR` — the last one lines up with the existing `pco_nir_lower_null_descriptors.c` approach.
- Still unknown, and the thing that decides the Mesa-side design: how the descriptor operand is encoded in bindless mode (memory load vs indexed entry), and where the mode flag is set. Next step is to find the option/mode setter and the operand encoding; until then M19 has no code.

---

## 2026-11-13 — M19 stage 1 continued, M20 8-bit

### M19: where the bindless constraint lives

`binaries/libusc.so` carries 130 embedded vendor source paths, which turns the diagnostics into source references. The only bindless message — `In bindless mode the last gather4 source must be a channel select.` (VA `0x27148`) — is attributed to `compiler/usc/volcanic/texture/usctexture.c`, i.e. the **Volcanic texture ISA emitter**, not a Vulkan-layer or driver file.

Its validation site is `0x215144`-`0x216240`, and the emitted checks are readable:

- `sub w8, w28, #0x6a; cmp w8, #0x3; b.hi ...` — opcode must be in `0x6a`-`0x6d`, the four `LDGATHER4*` variants (`LDGATHER4`, `LDGATHER4_C`, `LDGATHER4_PO`, `LDGATHER4_C_PO`).
- `adr x10, 0x287138; add x10, x10, x8, lsl #4` — a table of 16-byte input-instruction descriptors indexed by opcode; the neighbouring diagnostic names it `g_asInputInstDesc[psInputInst->eOpCode]`.
- `ldr w8, [x8, w9, uxtw #2]` against `adr x8, 0x8000 + 0x60` — a per-operand dword table.
- `cmp w8, w9` against `uNumSrcArgs` (`[x10, #4]`) → the `channelSelectSource < ...uNumSrcArgs` diagnostic.
- `mov w9, #0x2c; umaddl x8, w8, w9, x10; ldr w9, [x8, #4]; cmp w9, #0x1c; b.ne <error>` — operand descriptors are 44 bytes, their type field is at `+4`, and type `0x1c` (28) is the channel-select kind; anything else in the last gather4 source slot produces the bindless diagnostic.

So bindless is an **operand-encoding mode of the ISA emitter**: the descriptor travels in the shader's own operand stream and gather4's last source slot is constrained to a channel select. That rules out "generate dynamic PDS programs" as the shape of the fix for our driver and keeps the descriptor-memory-load plan (M19 stage 2-3) alive.

The mode flag itself is still unlocated — no vendor module contains a string naming it, so it lives in a struct field. Next step is to find the option struct that reaches `usctexture.c` (the setter is likely in the compile-options path, not in this file).

#### The input-instruction descriptor table, decoded

`g_asInputInstDesc` lives in `.data.rel.ro` at VA `0x287138` (file offset `0x267138`), 16-byte entries. The four gather4 opcodes read:

| Opcode | `uNumSrcArgs` |
| :--- | :---: |
| `0x6a` | 2 |
| `0x6b` | 3 |
| `0x6c` | 4 |
| `0x6d` | 2 |

The validator's last test is `operand[select].uType == 0x1c` on a 44-byte operand descriptor, where `select` is a channel-select source index that was range-checked against `uNumSrcArgs`. So the emitter knows, per gather4 variant, how many sources the instruction carries, and requires that the final source slot — in bindless mode — is a channel select. Which opcode maps to which `LDGATHER4*` spelling is not yet established; the operands themselves live in the shader's operand stream, not in a PDS entry.

#### Tooling added

`research/xref_string_aarch64.py`: resolves a string to its section and VA, then scans the disassembly for the `ADRP`/`ADD` (or `ADR`) pair that materialises it and prints the surrounding instructions. It reproduces the `0x216224` reference above, so the next stage does not have to redo this by hand.

#### Where the flag is *not*

The neighbouring diagnostics in the same `.rodata` block are assert expressions rather than messages, and they name the vendor's own structures: `uChannelSelectSource < g_asInputInstDesc[psInputInst->eOpCode].uNumSrcArgs`, `g_psInstDesc[psInstSt_Texel->eOpcode].eType == USC_INST_TYPE_LDST`, and `psState->uFlags2 & USC_FLAGS2_ASSIGNED_PREDICATE_REGNUMS`. That last one was the best candidate for a mode flag, so its family was enumerated: `libusc.so` carries 14 distinct `USC_FLAGS*` names and every one of them is compiler-internal state (SSA form, reg-num assignment, CFG frozen, work-list active, ...). None is a bindless or descriptor-indexing option, so bindless is not encoded in `uFlags2`.

Exported entry points that bound the remaining search: `PVRUniFlexCompileToHw` (compilation), `PVRUniFlexGetInputInstDesc` / `PVRUniFlexInitInst` (the input-instruction descriptor table decoded above), `PVRUniFlexSymbolicDebug`, `PVRUniflexReadHWBinary`. The option therefore arrives through the context/compile-info struct passed to `PVRUniFlexCompileToHw`; finding it means diffing the struct layout against what the Vulkan driver writes, which is the next step and is bounded (unlike grepping strings, which is now exhausted).

### M19 stage 2: what `PVRUniFlexCompileToHw` does with its arguments

`PVRUniFlexCompileToHw` sits at `0x6b438` and is a single ~37 KB function (`0x6b438`-`0x7459c`), so it is the whole compile pipeline rather than a wrapper. Its prologue fixes the argument roles:

| Register | Argument | Evidence |
| :--- | :--- | :--- |
| `x0` | context | `ldp x0, x8, [x0]` + `blr x8` at entry (vtable call), then `str x0, [x27, #16]` |
| `x1` | second struct | read at `+0`, `+24`; `ldrb` at `+34`; stored back at `+0` and `+24` |
| `x2` | third struct | used as an index base for byte fields at `+140`/`+141` |
| `x3` | options struct | double at `+0`, words at `+4` and `+8`, then `+44`, `+112`, `+4048`-`+4104` |
| `x4` | output/state struct | large, written at `+0`, `+40`-`+96`, `+3616`-`+3664` |

Two results that matter more than the map itself:

- **The gather4 check is ungated.** Between the opcode range check (`0x21514c`) and the operand-type compare (`0x21519c`) there is no flag load and no conditional branch other than the index bounds checks. So "bindless mode" is not a runtime switch consulted at emission — it is a *code path*: whoever builds the gather4 instruction chooses the operand form, and this emitter path is the bindless one. For our driver that means the lowering has to produce the bindless operand form, not toggle a bit.
- **Naive offset scanning does not survive this function.** The argument registers are reused as general temporaries a few hundred instructions in (`x20` becomes `x17+x5` at `0x6ccc0`, `x19` is reloaded from the stack at `0x6e43c`), so byte-offset "candidates" harvested by pattern matching cannot be attributed to an argument. The next attempt needs to follow the call chain that constructs the texture instruction — or a dataflow-capable disassembler — rather than regex over objdump output.

### M19 stage 3: following the gather4 emitter instead

Following the call chain rather than scanning offsets works, and it lands on the answer. The function that contains the gather4 validation spans `0x213120`-`0x2162f8` (12,472 bytes) and has exactly **one caller**, `0x1e8edc` inside the function starting at `0x1e8ea4`. That caller is the gather4 emitter:

1. It takes the component mask `ldrb w25, [x20, #16]` and a base index from the `0x213120` call (returned through `w24`), then loops `w22 = 0..3` over the four components of that mask.
2. Per present component it allocates an operand (`bl 0xabe18`) and types it via `0xabb84`. That function is "set operand type": the operand-type table lives at VA `0x289470` with 40-byte entries (`umaddl x8, w2, #40, x23` at `0xabba4`), and index `0x1c` is therefore a real operand type.
3. It then calls `0x1e1370` per component, passing a bit taken from the state word: `ldr w8, [x20, #692]` + `ubfx w4, w8, #1, #1` at `0x1e8f24`-`0x1e8f3c`. `0x1e1370` masks it to one bit (`and w6, w4, #0x1`) and forwards it to `0x1e1470`.
4. `0x1e1470` creates the **descriptor-index operand with category `0x1c`** — the channel-select category the gather4 validation demands — and then sets three operand-flag bits from three separate booleans (`0x1e154c`-`0x1e1588`):

| Flag | Set from |
| :--- | :--- |
| `0x20` | `tst w23, #1` — the `[state+692]` bit 1 propagated down from the gather4 emitter |
| `0x800` | `tst w24, #1` — `ldrb [x3, #40]` in the operand-declaration struct |
| `0x1000` | `tst w28, #1` — `ldrb [x3, #41]`, which additionally sets `0x200000` in the context word at `[x20, #12]` |

**Conclusion.** The flag we were hunting is not a compile-info option that gates emission — it is an *operand* flag on the descriptor-index operand, and the operand is always built with category `0x1c` (channel select). So there is nothing for our driver to switch on: to use bindless descriptors the emitter must produce the category-`0x1c` operand carrying the dynamic index, and set `0x20` on it when the index comes from a descriptor array. That is precisely the shape `pco_nir_vk.c` already computes for us — `load_vulkan_descriptor` lowers to `vec3(packed_desc, elem, 0)` and `fetch_resource_base_reg()` turns `desc_set`/`binding`/`elem` into a register range — which settles the M19 design question in favour of the descriptor-in-memory lowering.

### M19 stage 4: the opcode table, and a correction to stage 3

Stage 3's reading of the emitter was right about the mechanics and wrong about which opcode it was looking at. Two new tools (`research/jump_table.py`, `research/trace_regs.py`) settled both.

**`trace_regs.py`** is a symbolic register tracer: it walks a function linearly carrying `arg+offset` provenance, drops state at branch targets (conservative merge), and reports memory accesses and call arguments with the argument they belong to. On the gather4 emitter it attributes `[x20, #692]` and `[x20, #16]` to the second parameter and nothing else — the previous pattern-based pass had produced false hits once the argument registers were recycled.

**`jump_table.py`** decodes the dispatcher jump tables. Both dispatchers are the same shape:

```
ldrh w11, [x9, x8, lsl #1]    ; 16-bit offset
adr  x10, <base>
add  x10, x10, x11, lsl #2
br   x10
```

Dispatcher A (table at `0xa088`, base `0x1eb6cc`, guard `opcode <= 0x184`) maps **opcodes `0x60`-`0x6e` to the emitter at `0x1e8ea4`**. Dispatcher B reaches the same emitter only through an out-of-range fallback, so A is the real path.

**The correction.** `.data.rel.ro` holds a per-opcode record table with 16-byte entries at `0x287138`: `{u32, u32, char *name}`, indexed directly by opcode (389 entries, names resolved through `.rela.dyn` `R_AARCH64_RELATIVE` addends). Reading it gives the family the emitter actually serves:

| Opcode | Mnemonic | dst | src |
| :--- | :--- | ---: | ---: |
| `0x60` | `LD` | 1 | 6 |
| `0x63` | `LDL` | 1 | 7 |
| `0x69` | `LDLOD` | 1 | 6 |
| `0x6a` | `LDGATHER4` | 1 | 7 |
| `0x6b` | `LDGATHER4_C` | 1 | 8 |
| `0x6c` | `LDGATHER4_PO` | 1 | 7 |
| `0x6d` | `LDGATHER4_C_PO` | 1 | 8 |
| `0x6e` | `LD2DMS` | 1 | 8 |

So the special-cased `cmp w8, #0x69` path at `0x1e8f70` is **`LDLOD`**, not gather4. The `word0 = 13` / index `0` encoding that components 2 and 3 get there is LDLOD-specific and has nothing to do with channel select. Stage 3's table row for that path was mislabelled; the flag analysis above is unaffected, because the category-`0x1c` operand is built on the *common* per-component path (`0x1e8f18` → `0x1e1370` → `0x1e1470`) that every opcode `0x60`-`0x6e` takes.

**What this means concretely:** the channel-select form is not conditional on a gather4-specific test at all. For every LD-family instruction, each present component of the state mask (`[state+16]`, bits 0-3) gets one operand, and that operand is built with category `0x1c`. The only per-instruction variation is in its flag bits. The validator at `0x215144` therefore does not guard a mode — it guards that the emitter produced the operand shape the ISA requires, which is exactly what the unconditional path does.

### M19 stage 5: who writes bit 1 of `[state+692]`

Short answer: nothing writes that word in isolation. The evidence, in the order it closes doors:

1. **It starts at zero.** `PVRUniFlexInitInst` (`0x2052ac`) is three instructions of substance: `memset(inst + 4, 0, 0x2d4)` then `inst[0] = 0xffffffff`. So the whole instruction, including offset 692, is zeroed on init and the default is "flag clear". The same function gives the instruction size: 4 + 724 = 728 bytes, matching the `mov w1, #0x2d8` (728) allocation immediate in the constructor.
2. **No store targets it.** Scanning every store whose address range covers `[base+692, base+696)` — including `strb`/`strh` and 8-byte `str` at `+688` — yields exactly two 4-byte writers (`0x1916d0`, `0x191784`), and both use a base of `object+20`, i.e. they are writing `+692` of a *different* object (the `count * 216` bookkeeping array). Nothing writes this field directly.
3. **It arrives inside a copied pair.** The instruction constructor (`0x1d6504`, and siblings `0x1d65d8`, `0x1d8858`, `0x1d8944`) allocates 728 bytes, then writes `opcode` at `+0`, the component mask byte at `+16`, and copies an 8-byte field verbatim from the source/declaration object:

```
1d6510: ldr x8, [x19, #688]
1d6518: str w26, ...            ; mask byte at +16
1d651c: str x8, [x0, #688]      ; +688..+695, so +692 is the high word
```

Offset 692 is therefore the high word of that pair, not a field with its own writer. The bit is inherited from whatever object the instruction was built from, and that object's copy is opaque to static reading.
4. **It is consumed as a single bit.** Of the 46 sites that read the word, 44 are `ubfx wN, w8, #1, #1` and two read the whole value; no other bit of it is ever tested. So this is a dedicated boolean in the struct, not a general flags word.

**What this establishes for M19.** The mapping is now pinned end to end *inside the emitter*: bit 1 of `[inst+692]` → `ubfx w4, w8, #1, #1` at `0x1e8f3c` → `and w6, w4, #0x1` in `0x1e1370` → `tst w23, #1` at `0x1e1550`, which selects operand flag **`0x20`** on the category-`0x1c` operand. What it does *not* establish is the name of the condition: the value is copied opaquely from the declaration object's `+688` pair, so "bit 1 means bindless" remains an inference from context (it is the per-source boolean the emitter threads into the descriptor-index operand) rather than something the disassembly states. Settling that needs ground truth from the device — one shader compiled with a bindless descriptor and the resulting `+692` value read back.

Caveat worth keeping: which of the three bits is *the* bindless bit is inferred from code structure, not from a named option. `0x20` is the one fed by the instruction-state path that the gather4 emitter drives; `0x800`/`0x1000` come from the operand declaration and are more likely the uniform/dynamic-index pair. Confirming this needs one shader compiled by the vendor driver on the device, which is still gated.

### M20: 8-bit integers

- `shaderInt8` is now advertised. `pco_lower_bit_size_cb()` widens 8-bit ALU to 32 bits through `nir_lower_bit_size`, and the pass is placed **before** `nir_lower_mem_access_bit_sizes` so an 8-bit value feeding a load offset or store value is already 32-bit when accesses are sized.
- 8-bit storage stays off, and not because of silicon: `trans_load_buffer()` asserts `bits == 32` and `trans_store_buffer()` has a `switch (bits)` with only `case 32:` followed by `UNREACHABLE`, so a byte access would abort compilation. The vendor reports `storageBuffer8BitAccess = 1.0`, so the gap is ours.
- What the ISA does allow (from `pco_isa.py`): store instructions carry a `dsize` field with `8bit`/`16bit`/`32bit` encodings, so byte *stores* are expressible. Load instructions (`I_LD_IMMBL`, `I_LD_REGBL`) declare no `dsize` — only `burstlen` in 128-bit units — so there is no byte-load encoding in the generated ISA description. That asymmetry is the concrete reason both 16-bit and 8-bit *storage* features stay off: they need byte/word loads, and no lowering in PCO currently synthesises them from a wider access. Resolving it needs either the missing load encoding or a lowering that reads a containing word, which is only sound when alignment and bounds allow it.
- Gate: `test_int8_lowering.py` (10 checks, mutation-probed) enforces the widening pass, its ordering, the feature state, **and** that 8-bit storage is not advertised while the backend is 32-bit-only. Flipping `storageBuffer8BitAccess` on fails two checks.

---

## 2026-11-18 — int8 lowering convergence fix

### The defect

`pco_lower_bit_size_cb()` widened every ALU instruction whose result was 8-bit, including conversion opcodes (`u2u8`, `i2i8`, `f2f16`). `nir_lower_bit_size()` cannot widen those: their source is already 32-bit, so `convert_to_bit_size()` would assert, and the pass ends up rebuilding the same instruction with an 8-bit result. The rebuild is reported as progress, so the `pco_nir_opt()` do/while never reached a fixed point and only exited through its `count > 1000` guard, printing `WARNING! Infinite opt loop!` and re-emitting dead conversions on every iteration.

Conversion ops whose *def* is 8-bit cannot come from a 16-bit source, so no narrowing was ever lost — this is a compile-time defect (loop never converges, log spam, wasted iterations per shader), not a wrong-result one.

### The fix

`pco_nir.c`: the callback now returns 0 for any op with a sized output type (`nir_op_infos[op].output_type`), i.e. every conversion. Sized output is exactly the condition the pass itself uses to decide whether a result has to be converted back, so non-conversion 8-bit ALU (arithmetic, logic, moves, selects) is still widened.

### Validation

```bash
ninja -C build-android src/imagination/vulkan/libvulkan_powervr_mesa.so   # links, 7,037,984 bytes
python3 test_int8_lowering.py                                            # 10/10
```

Mutation probe: deleting the output-type guard fails exactly the new check (9/10), the source was restored byte-identically (`cmp`), and the gate returned to 10/10.

Full sweep re-run after the change: multiviewport 9/9, int64 21/21, ASTC 156, depth clamp 19/19, volcanic mock 8/8, header validation 10/10, M19 opcode tables 10/10, BC image path 22/22, `test_bridge_v25_mock` and `test_drm_shim_mock` passed from fresh builds, `test_bc_decode` 45/45, and `tools/package_driver.py` rebuilt both zips (7,037,984-byte library, exports and `DT_NEEDED` revalidated).

### Outcome

The int8 path now converges. This does **not** unblock 16-bit features: `shaderInt16` / `shaderFloat16` need the same widening plus a plan for comparisons, whose result is 1-bit, so `nir_lower_bit_size()` never sees them and 16-bit operand refs would reach a backend that assumes 32-bit ALU operands. Whether the backend can compare 16-bit operand refs is unverified, and that is the prerequisite.

---

## 2026-11-18 — 16-bit shader ALU: does trans_cmp() accept 16-bit operands?

### The question, answered

No. `trans_cmp()` (`pco_trans_nir.c`) builds `tst_op_main` from the opcode and `tst_type_main` from `to_tst_type_main()`, which maps opcodes to `PCO_TST_TYPE_MAIN_F32`, `S32` or `U32` and nothing else; the operand refs are then handed straight to `pco_bcmp()`/`pco_scmp()`. There is no `F16`/`S16` test type in the ISA description, so a 16-bit ref would be compared as a 32-bit register with whatever happens to be in the upper half. Advertising `shaderInt16`/`shaderFloat16` on that path would have produced silently wrong comparisons.

The second half of the problem is that the existing widening pass could never have fixed it: `nir_lower_bit_size()` selects instructions by their *result* width, and a comparison's result is a 1-bit bool.

### Implementation

* `pco_nir.c`: the callback now widens 16-bit ALU in addition to 8-bit, and — because a comparison's result width says nothing about its operands — also returns 32 for any 1-bit-result ALU op with an 8- or 16-bit operand. This closed the same hole in the already-advertised `shaderInt8` path, where an 8-bit comparison was reaching the backend unchanged.
* `pco_trans_nir.c`: narrowing integer conversions (`i2i16`, `u2u16`) truncate with a `0xffff` mask. `pck` looks tempting (`PCO_PCK_FMT_S1616`) but quantises: after widening, an add that wraps past `INT16_MAX` arrives as a 32-bit value and would be clamped rather than wrapped. Widening (`i2i32` from a 16-bit lane) sign-extends by unpacking the low lane as `S1616`, which cannot saturate; `u2u32` is already zero-extended by the mask. `trans_cmp()` now asserts the 32-bit operand invariant where a reader would look for it.
* `pvr_physical_device.c`: `shaderInt16` and `shaderFloat16` are advertised only when `PVR_SHADER_16BIT=1`. 16-bit storage (`storageBuffer16BitAccess`, `uniformAndStorageBuffer16BitAccess`, `storagePushConstant16`, `storageInputOutput16`) stays off: the backend still has only 32-bit loads and stores.

Float16 needs no conversion work: `f2f16`/`f2f32` already pack and unpack the half lane, so the widened arithmetic is exact at f32 and rounded once when packed (permitted, not bit-exact against native half arithmetic).

### Validation

```bash
ninja -C build-android src/imagination/vulkan/libvulkan_powervr_mesa.so   # links, 7,038,456 bytes
python3 test_shader_16bit.py                                            # 15/15
python3 test_int8_lowering.py                                           # 10/10
```

Mutation probes: dropping `|| alu->def.bit_size == 16` fails one check (14/15); setting `.shaderInt16 = true,` unconditionally fails one check (14/15). Sources restored byte-identically (`cmp`), gate back to 15/15.

Full sweep: int64 21/21, multiviewport 9/9, ASTC 156, depth clamp 19/19, volcanic mock 8/8, header validation 10/10, M19 opcode tables 10/10, BC image path 22/22; `test_bridge_v25_mock`, `test_drm_shim_mock` pass from fresh builds; `test_bc_decode` 45/45; `tools/package_driver.py` rebuilt both zips (2,019,443 / 2,019,369 bytes) and revalidated exports and `DT_NEEDED`.

### Outcome

16-bit shaders now translate instead of aborting or mis-comparing, but the features are gated: nothing has been executed on the device. Two limits are named in the gate and in the changelog — float16 arithmetic is computed at f32 and rounded once, and 16-bit `phi` results stay 16-bit because the pass's phi lowering is integer-only, so a value crossing a block boundary is consumed through a pack/unpack pair. Removing `PVR_SHADER_16BIT` needs a 16-bit shader whose output is compared against the vendor driver on the phone.

---

## 2026-11-18 — Why the emulator never started: the required-feature list

### The finding

Every milestone so far was judged by whether a feature worked. That was the wrong question to answer first. Yuzu and Eden score each physical device and reject one that is missing a feature they require; a log from such a device lists:

```
Missing required feature uniformAndStorageBuffer16BitAccess
Missing required feature storageBuffer8BitAccess
Missing required feature uniformAndStorageBuffer8BitAccess
Missing required feature vertexPipelineStoresAndAtomics
```

with `independentBlend` made required alongside them. This driver reported **all of those as `false`**, so the emulator never selected it, never compiled a shader, and never started a game — no matter how correct ASTC, depth clamp, multi-viewport, int64 or 16-bit ALU were. The vendor driver reports `1.0` for every one of them on this same silicon, so the gap was entirely ours.

### Why each one was off, and what made it truthful

* `vertexPipelineStoresAndAtomics`: the translation is already stage-agnostic (`trans_store_buffer()`, `trans_global_atomic_buffer()` take no stage), so the bit was simply never revisited upstream. Advertised.
* 8- and 16-bit storage: `pco_mem_access_size_align()` used to *lower the request* to an 8- or 16-bit access when the member was only 1- or 2-byte aligned, which `trans_load_buffer()` (asserts `bits == 32`) and `trans_store_buffer()` (`UNREACHABLE`) cannot encode. It now asks for an aligned 32-bit word instead, and `nir_lower_mem_access_bit_sizes` does the work it already knows how to do: a narrow load becomes word load + byte shift + bitfield extract, a narrow store becomes the `iand`/`ior` pair of 32-bit atomics it emits for unaligned stores (`may_lower_unaligned_stores_to_atomics`). `to_atom_op()` already mapped both to `PCO_ATOM_OP_AND`/`OR`, and `pco_nir_algebraic.py` already lowered `extract_u8/u16` to bitfield extracts.
* `shaderInt16`/`shaderFloat16`: 16-bit storage means halves arrive from memory whether or not the shader bits are advertised, so the default flipped from off to on (`PVR_SHADER_16BIT=0` disables just those two for bisection; the widening itself is unconditional).

### Validation

```bash
ninja -C build-android src/imagination/vulkan/libvulkan_powervr_mesa.so   # links, 7,038,512 bytes
python3 test_switch_required_features.py                                 # 28/28
python3 test_int8_lowering.py                                            # 13/13
python3 test_shader_16bit.py                                             # 15/15
```

Mutation probes: putting the narrow request back to 16 bits fails 1/28 and 1/13; `.vertexPipelineStoresAndAtomics = false` fails 2/28; dropping the atomics fallback fails 1/28. Sources restored byte-identically (`cmp`), gate back to 28/28.

Full sweep: int64 21/21, multiviewport 9/9, ASTC 156, depth clamp 19/19, volcanic mock 8/8, header validation 10/10, M19 opcode tables 10/10, BC image path 22/22; `test_bridge_v25_mock`, `test_drm_shim_mock` pass from fresh builds; `test_bc_decode` 45/45; `tools/package_driver.py` rebuilt both zips (2,019,544 / 2,019,470 bytes) and revalidated exports and `DT_NEEDED`.

### Outcome

The emulator's device-selection list is now satisfied, so the driver can reach shader compilation instead of being skipped. Still unverified on silicon: the narrow-store mask-and-merge path (two concurrent narrow stores to one word interleave per operation), and whether the firmware lets a vertex program write global memory beyond the vendor's reported `1.0`. `storagePushConstant8/16` and `storageInputOutput16` remain off; they need byte/word push-constant upload and 16-bit varyings.

---

## Notes on Execution

This log is intentionally designed to be transparent and auditable. The working assumption is:

- if the project has a hazard or safety boundary, it is logged before code changes are attempted;
- if the milestone is implemented, the validation command and result are retained here;
- if the milestone is not yet implemented, the current blockage is stated plainly and the next step is named.

This preserves continuity and allows future sessions to resume without losing the current state.
