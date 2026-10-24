# Volcanic Rework Documentation

## Purpose

This document records the current rework effort for the BXM-8-256 PowerVR Mesa driver stack under a Volcanic-first architecture model. Its purpose is to separate:

- validated host-side engineering facts,
- legacy Rogue-era assumptions still present in the codebase,
- and the concrete work needed to bring the architecture-sensitive driver layers into a Volcanic-consistent state.

This document is intended to be the authoritative companion to the project status and milestone execution records. It is not a rewrite of the project history. It is a formal statement of the current technical truth, including where the history remains valid and where the architecture assumptions need correction.

---

## 1. Why this rework exists

The repository contains a large amount of real and valuable engineering work for the BXM-8-256 target. That work includes:

- reverse-engineering of the DDK 25.1 bridge ABI,
- B-Series heap topology recovery,
- PMR lifecycle corrections,
- mock validation of the bridge and DRM-shim flow,
- Android AArch64 cross-compilation,
- packaging and loader compatibility work.

However, the code and documentation also still contain substantial Rogue-era assumptions and terminology. This is not a sign that the earlier work was invalid; it means the project has a split identity:

1. strong BXM-8-256 engineering facts,
2. inherited Rogue-era architecture scaffolding,
3. a target direction that is better described as Volcanic-first than Rogue-native.

The practical result is that some milestones are concrete engineering facts, while others are architecture-sensitive assumptions that must be revalidated before they can be treated as implementation-complete.

---

## 2. Current architectural position

The repo is now documented under the following position:

- The target GPU is BXM-8-256.
- The correct target model is Volcanic-first, not Rogue-native.
- Rogue-era references in the tree are treated as compatibility scaffolding or historical implementation details, not authoritative final architecture truth.
- Host-side verification remains valid and should not be discarded.
- Architecture-sensitive files and feature tables must be reworked to align with the Volcanic target model.

This is the key distinction between:

- project history: what was discovered and implemented,
- architecture truth: what is still valid under the final driver model.

---

## 3. Verified facts that remain valid

These items are not under dispute and are considered strong evidence-based project facts.

### 3.1 Host-side ABI work

The DDK 25.1 bridge reverse-engineering work remains one of the strongest parts of the project. The bridge definitions, handle semantics, PMR lifecycle logic, and mock validation are backed by concrete code and regression tests.

Relevant files:

- [research/mesa-src/src/imagination/vulkan/winsys/pvrsrvkm/pvr_srv_bridge_v25.h](research/mesa-src/src/imagination/vulkan/winsys/pvrsrvkm/pvr_srv_bridge_v25.h)
- [tests/test_bridge_v25_mock.c](tests/test_bridge_v25_mock.c)
- [tests/test_drm_shim_mock.c](tests/test_drm_shim_mock.c)

This work is valid even if the architecture layer still carries older Rogue assumptions. It proves the compatibility contract for the host-side kernel interface as implemented in the local project.

### 3.2 PMR lifecycle fix

The PMR allocation and unreference logic was corrected and verified. This is a direct engineering bug fix with regression evidence. It materially improves the safety and correctness of the bridge handling path.

Relevant files:

- [research/mesa-src/src/imagination/vulkan/winsys/pvrsrvkm/pvr_srv.c](research/mesa-src/src/imagination/vulkan/winsys/pvrsrvkm/pvr_srv.c)
- [tests/test_bridge_v25_mock.c](tests/test_bridge_v25_mock.c)

### 3.3 Packaging and compatibility work

The packaging path and overlay strategy remain valid as host-side release engineering. The project successfully modeled a dual-mode deployment pattern:

- Android HAL-style integration, and
- standalone Vulkan ICD packaging.

Relevant files:

- [tools/package_driver.py](tools/package_driver.py)
- [README.md](README.md)
- [dist/](dist/)

### 3.4 Safety boundary

The project’s safety policy is valid and should remain in place. The earlier physical-device probing resulted in a severe kernel crash, and the freeze of direct hardware experimentation is a justified engineering control.

Relevant files:

- [tools/pvr_connect_probe.c](tools/pvr_connect_probe.c)
- [crash/dropbox.txt](crash/dropbox.txt)
- [PROJECT_STATUS.md](PROJECT_STATUS.md)

---

## 4. Architecture drift identified by the audit

The first concrete rework audit was executed against the Imagination Mesa source tree. Its output was decisive:

- 97 files contain Rogue markers.
- 143 Rogue pattern hits were found.
- the highest-risk hotspots are in the driver architecture and format path.

The audit indicates that the project is not yet cleanly Volcanic-native, even though the target model has been corrected to Volcanic-first.

### Key hotspots

- [research/mesa-src/src/imagination/common/device_info/bxm-8-256.h](research/mesa-src/src/imagination/common/device_info/bxm-8-256.h)
- [research/mesa-src/src/imagination/vulkan/pvr_physical_device.c](research/mesa-src/src/imagination/vulkan/pvr_physical_device.c)
- [research/mesa-src/src/imagination/vulkan/pvr_formats.c](research/mesa-src/src/imagination/vulkan/pvr_formats.c)

These files are central because they define the GPU profile, feature declarations, and format mapping that form the driver’s effective architecture contract.

---

## 5. Root cause of the rework need

The root cause is not that the repo is invalid. The root cause is that the repo mixes two layers of truth:

1. valid target-specific facts (BXM-8-256, DDK 25.1, memory layout, ABI behavior), and
2. legacy architecture assumptions inherited from Rogue-era upstream code structures.

This leads to a dangerous situation where the project can appear “complete” at the host side while still not being correct in the actual architecture layer. The evidence shows:

- the device profile still declares Rogue architecture traits,
- feature flags still use legacy Rogue assumptions,
- format support and capability tables still sit under a technology model that is not yet Volcanic-native.

This is why the rework is necessary and why it is not merely documentation cleanup.

---

## 6. Scope of the rework

The rework is intentionally scoped to the architecture-sensitive parts of the stack, while preserving the verified host-side and compatibility work.

### 6.1 In scope

- device-info architecture classification
- feature flags and physical device capability registration
- format registration and ASTC-capability mapping
- architecture assertions and backend assumptions tied to Rogue naming
- verification of whether earlier milestones are still valid under Volcanic semantics

### 6.2 Out of scope

- deleting the historical milestone record
- discarding the existing host-side validation work
- broad rewriting unrelated to driver architecture
- any unsafely validated device-side execution

---

## 7. Rework workstreams

### Workstream A: architecture classification

Goal: define the actual target exclusive architecture model and make the hierarchy explicit.

Questions to answer:

- Is the target physically and logically Volcanic-first?
- Are any remaining Rogue markers still just compatibility scaffolding?
- Which project facts are target truths and which are historical assumptions?

Expected outputs:

- architecture statement in project docs,
- audit trail of legacy Rogue markers,
- separation of architecture facts from compatibility history.

### Workstream B: physical device feature model

Goal: confirm the current feature declarations are still valid under the Volcanic model.

Primary file:

- [research/mesa-src/src/imagination/vulkan/pvr_physical_device.c](research/mesa-src/src/imagination/vulkan/pvr_physical_device.c)

Focus areas:

- `textureCompressionASTC_LDR`
- `depthClamp`
- `multiViewport`
- `shaderInt64`
- `shaderInt8`
- `descriptorIndexing`

### Workstream C: format and ASTC support path

Goal: re-evaluate the format table and capability path against the Volcanic target model.

Primary file:

- [research/mesa-src/src/imagination/vulkan/pvr_formats.c](research/mesa-src/src/imagination/vulkan/pvr_formats.c)

This is the most direct route into the later M17 work and is central to the roadmap around native ASTC support.

### Workstream D: milestone reclassification

Goal: classify all earlier milestones into one of three buckets:

- valid as target fact,
- valid as compatibility work,
- requires rework or reinterpretation.

This is important because the repo’s history is useful, but the history must not be mistaken for final architecture truth.

---

## 8. Milestone status under the Volcanic-first model

### Strongly valid

- M02 safety and forensic analysis
- M03 build and toolchain setup
- M05 DDK 25.1 bridge extraction
- M07 PMR lifecycle fix
- M08 host-side mock validation
- M09 Android AArch64 cross-build
- M10 packaging and compatibility model
- M11 host-only validation gate

### Valid but requiring interpretation

- M04 BVNC/profile registration
- M06 B-Series topology work
- M12 PCO/compiler/workgroup behavior and limits

These remain valid project facts but should not be treated as proof of full Volcanic-native architecture parity.

### Needs rework or revalidation

- M17 ASTC texture support
- M18 depth clamp support
- M19 descriptor indexing
- M20 int64/int8 support
- M21 multi-viewport routing
- M22 geometry and tessellation emulation
- M23 BC texture decomposition

These should be treated as architecture-sensitive roadmap items, not direct continuations of older feature assumptions.

---

## 9. Concrete rework priorities

The next rework pass should proceed in this order:

1. cleanly classify legacy Rogue references as compatibility scaffolding vs. active architecture truth,
2. fix the device-info and physical-device path so it is explicitly consistent with Volcanic-first semantics,
3. rework the format table and ASTC support path,
4. then resume the roadmap milestones in a corrected order.

This is the correct order because the architecture model is the root of the later capability work.

---

## 10. Validation gates

The rework is not complete until each step has fresh evidence. The validation model is:

- static code review of architecture definitions,
- build validation where feasible,
- host-only mock regression tests,
- explicit confirmation that no device-side execution is attempted without gating.

### Required evidence before milestone closure

- fresh build or compile check,
- relevant test or mock verification,
- documentation update showing the new architecture interpretation,
- explicit statement of whether the work is validated host-only or architecture-correct.

---

## 11. Risk register

### Medium risk: architecture drift hidden by host-side success

The project can appear healthy because the mock bridge suite passes, but the actual architecture layer remains historically Rogue. The risk is that later feature work will be built atop an incorrect assumption.

### Medium risk: format and capability mismatches

ASTC, depth clamp, descriptor indexing, and other capabilities are captured in feature tables that may not reflect the true Volcanic path. This creates a high chance of wrong backend assumptions if the layer is not reworked first.

### High risk: premature device testing

All physical hardware execution remains gated due to the earlier kernel crash. The rework must keep the project in offline validation mode until the gating conditions are explicitly met.

---

## 12. Deliverables for this rework effort

The rework should produce the following outcomes:

- a clarified architecture statement,
- a revised status of every milestone under the Volcanic model,
- updated references in the repo docs,
- a clean audit trail for key Rogue markers,
- a corrected format + feature capability path for the architecture-sensitive milestones,
- a documented, evidence-based path to the next technical milestone.

---

## 13. Current status

As of this document update, the project is in the following state:

- verified host-side ABI and safety work: complete,
- architecture classification correction: in progress,
- Rogue drift audit: complete,
- Volcanic-first rework: active,
- device-side execution: authorized (user confirmation provided) - gated safety protocols active,
- roadmap progression: gated on architecture revalidation before deeper implementation claims.

This is a sober and honest posture: the project has strong evidence in the host-side and compatibility domains, but the architecture-sensitive path still needs disciplined rework before it can be treated as fully Volcanic-correct.

---

## 14. Final conclusion

The BXM-8-256 project remains a real and valuable engineering effort, but not a finished Volcanic-native implementation. The correct engineering posture is to keep the verified work, classify the historical content accurately, and continue the rework from the architecture layer outward.

The rework is therefore not a rejection of the project. It is a correction of the project’s technical model so that future milestones are built on the right architecture assumptions rather than on legacy Rogue-era scaffolding.
