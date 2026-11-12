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

## Notes on Execution

This log is intentionally designed to be transparent and auditable. The working assumption is:

- if the project has a hazard or safety boundary, it is logged before code changes are attempted;
- if the milestone is implemented, the validation command and result are retained here;
- if the milestone is not yet implemented, the current blockage is stated plainly and the next step is named.

This preserves continuity and allows future sessions to resume without losing the current state.
