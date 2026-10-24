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

#### Current findings
- `textureCompressionASTC_LDR` is explicitly set to `false` in the physical device feature table.
- There are no ASTC entries in `pvr_formats.c` discovered in a targeted search.
- This indicates the feature is currently disabled by omission and not yet supported by the format registry.

#### Implementation plan for the next pass
1. Audit the format registration pattern in `pvr_formats.c` and identify the equivalent compressed texture registration model.
2. Add the supported ASTC Vulkan formats for the PowerVR driver.
3. Flip the physical device feature `textureCompressionASTC_LDR` to `true` only after format coverage is validated.
4. Run the relevant compile-check or targeted unit verification to confirm the change does not break the driver build.

#### Status
M17: investigation started; no patch yet.

---

## Notes on Execution

This log is intentionally designed to be transparent and auditable. The working assumption is:

- if the project has a hazard or safety boundary, it is logged before code changes are attempted;
- if the milestone is implemented, the validation command and result are retained here;
- if the milestone is not yet implemented, the current blockage is stated plainly and the next step is named.

This preserves continuity and allows future sessions to resume without losing the current state.
