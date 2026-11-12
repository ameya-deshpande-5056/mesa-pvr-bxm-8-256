# M19 & M22: What Blocks Them, and the Path Forward

> **Status:** analysis + plan only. No M19 or M22 code is enabled in the driver.
> Every feature flag covered here stays **off** until its milestone lands, because
> advertising a feature the driver cannot execute is worse than not having it:
> emulators branch on supported features and will take the path that then breaks.

Both milestones were investigated on 2026-11-12 after M20 and M21 landed. The most
important finding reframes both of them:

## 0. The hardware and the vendor driver support all of it

`device-info/vkjson.json` is a capability dump from the stock driver
(`vulkan.mtk.so`) on the target Redmi Note 14 5G / BXM-8-256. It reports:

| Capability | Vendor value | Meaning |
| :--- | :---: | :--- |
| `descriptorIndexing` | `1.0` | Bindless descriptors work on this silicon |
| `runtimeDescriptorArray` | `1.0` | Truly unbounded descriptor arrays work |
| `shaderSampledImageArrayNonUniformIndexing` | `1.0` | Per-lane texture index |
| `shaderStorageBufferArrayNonUniformIndexing` | `1.0` | Per-lane SSBO index |
| `descriptorBindingPartiallyBound` | `1.0` | Sparse descriptor arrays |
| `descriptorBindingVariableDescriptorCount` | `1.0` | Dynamic array sizes |
| `storageBuffer8BitAccess`, `shaderInt8` | `1.0` | 8-bit storage works |
| `geometryShader` | `1.0` | GS is emulated by the vendor driver |
| `tessellationShader` | `1.0` | Tessellation is emulated |

So M19 and M22 are **not** hardware walls. They are missing **Mesa-side
implementations**, and the stock DDK contains working ones to study. Anything in
this repository that says "the hardware cannot do X" for these two features is
wrong and should be corrected.

---

## 1. M19 — Descriptor Indexing / Bindless

### 1.1 Why the current driver cannot just switch it on

Descriptors do not live in memory from the shader's point of view. At pipeline
creation the driver generates a **PDS descriptor-upload program**
(`pvr_pds_descriptor_program_create_and_upload()` in
`src/imagination/vulkan/pvr_arch_pipeline.c`) from the descriptor-set layout. That
program copies each descriptor into **USC shared registers** at draw time, and the
compiler models a descriptor as a *shared-register base plus a uniform stride*:

* `fetch_resource_base_reg()` (`pco_trans_nir.c`) returns the shared-register base
  for `(desc_set, binding, elem)`.
* `lookup_load_tex_smp_state()` supports **dynamic** indexing by fetching element 0
  and adding `dynidx_elem * dynidx_stride`.
* `pco_pack_desc()` packs `(desc_set, binding)` into one immediate and
  `lower_load_vulkan_descriptor()` (`pco_nir_vk.c`) turns
  `load_vulkan_descriptor` into a `vec3(desc_set_binding, elem, 0)`.

Shared-register indexing is **uniform** (one index per task), which is exactly why
the driver can honestly advertise `shader*ArrayDynamicIndexing = true` and must
keep `shader*ArrayNonUniformIndexing = false`. A per-lane index has nowhere to go:
there is no descriptor to point at, only a register number.

This is also why the 2026-11-12 attempt to enable the feature set was reverted: it
flipped ten `descriptorBinding*` / `runtimeDescriptorArray` / non-uniform flags to
`true` while leaving `descriptorIndexing = false` (a combination the Vulkan spec
forbids), and its "bindless" lowering branch was an `if`/`else` with two identical
bodies.

### 1.2 Staged plan

**Stage 1 — Forensic study (no driver change).**
The DDK already does this, so find out how before writing code:

* Disassemble the descriptor/PDS side of `binaries/libsrv_um.so` and the compiler
  side of the vendor `libusc.so`: look for the descriptor-memory access pattern a
  bindless shader produces (a 64-bit pointer per descriptor plus a load, versus a
  PDS entry index).
* Cross-check against `pvrsrvkm.ko`'s feature tables for any per-BVNC bindless
  toggle, using `tools/lpdumps` and the existing disassembly scripts under
  `research/`.
* Deliverable: a note in `MILESTONE_EXECUTION_LOG.md` naming the mechanism
  (descriptor-in-memory load vs. indexed PDS entry) — everything below depends on
  which one it is.

**Stage 2 — Descriptor memory model (driver).**
`pvr_descriptor_set_layout_binding` already carries `offset` and `stride` within a
set, so the layout arithmetic exists. What is missing is a GPU-visible buffer per
set that the PDS program *and* the shader can both read, with the same layout the
PDS upload program consumes. Target files: `pvr_descriptor_set.c` (allocation and
write path), `pvr_arch_pipeline.c` (stop generating a per-binding upload entry for
bindings flagged `VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT`).

**Stage 3 — PCO lowering.**
Replace the `vec3(set_binding, elem, 0)` representation for runtime-array
descriptors with a real address: `set_base + binding_offset + index * stride`,
loaded with `nir_intrinsic_load_global`. This is the Turnip/radv shape of the fix
and the reason `pco_nir_lower_null_descriptors.c` (already present, used for
robustness2 null descriptors) is the natural place to extend.

**Stage 4 — Feature gating, one flag at a time.**
Enable in dependency order and gate each on the previous: `descriptorIndexing` →
`runtimeDescriptorArray` → non-uniform indexing → `partiallyBound` →
`variableDescriptorCount` → `updateAfterBind`. Note `descriptorIndexing` is the
umbrella bit and **must** be `true` whenever any sub-feature is (`test_int64_lowering.py`
has no opinion here, but the reverted attempt got this wrong).

**Deliberately out of scope:** `descriptorBindingUpdateAfterBind` needs an audit of
whether a descriptor set that is rewritten while referenced by an in-flight draw
can race the PDS execution. That is a correctness question about submission, not a
compiler question, and must be answered before it is advertised.

---

## 2. M22 — Geometry and Tessellation

### 2.1 Why it is a different kind of problem

Rogue/Volcanic fixed-function hardware has a VDM (vertex data master), a PDS and a
TA/ISP pipeline, but no geometry or tessellation units. The vendor driver reports
both stages and therefore emulates them. Mesa's PCO backend has **no** streaming
stage at all: it compiles one shader per stage into a single PDS program, with no
intermediate Parameter Buffer (PB) and no way to run a variable number of vertex
passes per input primitive.

### 2.2 Staged plan

**Stage 1 — Confirm the shape of the DDK emulation.** Disassemble the vendor
compiler's handling of a GS/tess pipeline: how many passes, where the intermediate
buffer lives (PB in the heap vs. on-chip), and how primitive count amplification is
propagated to the TA. This is the same forensic path as M19 Stage 1 and shares the
tooling.

**Stage 2 — PB allocation and indexing.** Add a per-draw PB region plus the address
arithmetic in PCO. Without Stage 1 this is guesswork; do not skip it.

**Stage 3 — Pass chaining.** Emit VDM/TA state that runs the vertex shader, then a
geometry pass over the PB, then the fragment pass, with the primitive count taken
from the previous pass rather than the draw call. This is the first point at which
`VkPipeline` needs to describe more than one shader per stage, which is a
driver-wide change (`pvr_arch_pipeline.c`, `pvr_arch_cmd_buffer.c`, shader state
shared registers).

**Stage 4 — Advertise** `geometryShader`, then `tessellationShader`, each gated on
its own smoke test.

### 2.3 Recommendation

M22 is a multi-week architecture project, not a milestone-sized task. The 720p/30
target for 2D indies, retro ports and lightweight JRPGs does not need GS or
tessellation: the Switch titles in scope do not use them, and DXVK falls back for
desktop GL titles. M19's `descriptorIndexing` matters far more for Winlator/DXVK
compatibility, and M20's remaining 8-bit storage is a much smaller win for far less
risk than either.

---

## 3. What is enforced today

* `descriptorIndexing = false`, `runtimeDescriptorArray = false`, every
  non-uniform-indexing and `descriptorBinding*` flag `false`.
* `geometryShader = false`, `tessellationShader = false`.
* `storageBuffer8BitAccess` / `uniformAndStorageBuffer8BitAccess` / `shaderInt8`
  `false` until a `nir_lower_bit_size` hook exists in PCO.
* `shaderBufferInt64Atomics` / `shaderSharedInt64Atomics` `false` (M20 covers
  arithmetic and memory, not atomics).
