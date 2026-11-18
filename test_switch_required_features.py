#!/usr/bin/env python3
"""
Switch-emulator gate: the features Yuzu/Eden require before they will use this driver.

Yuzu and Eden score every physical device and reject one that is missing a
required feature. The list is short and unforgiving -- a run against a Mesa
driver logs, in order:

    Missing required feature uniformAndStorageBuffer16BitAccess
    Missing required feature storageBuffer8BitAccess
    Missing required feature uniformAndStorageBuffer8BitAccess
    Missing required feature vertexPipelineStoresAndAtomics

plus `independentBlend`, which was made a required feature alongside them. If
any of these is 0 the emulator never selects the driver, so nothing else in
this project can matter: no shader is compiled, no game starts.

What makes each of them truthful here (and what this gate pins):

  * narrow storage (8- and 16-bit) is lowered by the memory-access pass to
    aligned 32-bit words: a narrow load becomes word load + shift + bitfield
    extract, and a narrow store becomes the iand/ior pair of 32-bit atomics
    that pco_trans_nir.c already translates (PCO_ATOM_OP_AND/OR). The callback
    must therefore never ask for an 8- or 16-bit access, and the atomics option
    must be enabled;
  * vertex-stage stores use the same stage-agnostic translation as compute and
    fragment (trans_store_buffer(), trans_global_atomic_buffer()).

The vendor driver reports 1.0 for all of these on this silicon, so none of it
is a hardware gap.
"""

import os
import sys

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
MESA_SRC = os.path.join(REPO_ROOT, "research/mesa-src")
VK_DIR = os.path.join(MESA_SRC, "src/imagination/vulkan")
PCO_DIR = os.path.join(MESA_SRC, "src/imagination/pco")

if not os.path.isdir(VK_DIR):
    print("SKIP: mesa-src is not initialised (git submodule update --init)")
    sys.exit(1)

# Feature name -> the literal the feature table must contain.
REQUIRED = {
    "independentBlend": ".independentBlend = true,",
    "vertexPipelineStoresAndAtomics": ".vertexPipelineStoresAndAtomics = true,",
    "storageBuffer8BitAccess": ".storageBuffer8BitAccess = true,",
    "uniformAndStorageBuffer8BitAccess": ".uniformAndStorageBuffer8BitAccess = true,",
    "storageBuffer16BitAccess": ".storageBuffer16BitAccess = true,",
    "uniformAndStorageBuffer16BitAccess": ".uniformAndStorageBuffer16BitAccess = true,",
    # Not part of the device-selection list, but a Switch shader that loads a
    # half or a short needs them, and the vendor reports both as 1.0.
    "shaderInt16": ".shaderInt16 = pvr_shader_16bit_allowed(),",
    "shaderFloat16": ".shaderFloat16 = pvr_shader_16bit_allowed(),",
    "shaderInt8": ".shaderInt8 = true,",
}

checks = []


def read(directory, name):
    with open(os.path.join(directory, name)) as f:
        return f.read()


def check(name, condition, detail=""):
    checks.append((name, condition, detail))


def main():
    pco_nir = read(PCO_DIR, "pco_nir.c")
    trans_nir = read(PCO_DIR, "pco_trans_nir.c")
    physical = read(VK_DIR, "pvr_physical_device.c")

    # 1. Every feature the emulator requires is advertised.
    for feature, literal in REQUIRED.items():
        check(f"{feature} is advertised", literal in physical)

    # 2. The gate that can still hide shaderInt16/shaderFloat16 must default on,
    #    otherwise the emulator sees 0 and rejects the device.
    gate_start = physical.find("static bool pvr_shader_16bit_allowed(void)")
    gate_body = (
        physical[gate_start:physical.find("\n}\n", gate_start)]
        if gate_start != -1
        else ""
    )
    check(
        "shaderInt16/shaderFloat16 are on by default",
        'debug_get_bool_option("PVR_SHADER_16BIT", true)' in gate_body,
    )

    # 3. Narrow storage is lowered to words, never handed to the backend.
    cb_start = pco_nir.find("pco_mem_access_size_align(")
    mem_cb = (
        pco_nir[cb_start : pco_nir.find("\n}\n", cb_start)] if cb_start != -1 else ""
    )
    check(
        "narrow accesses request an aligned 32-bit word",
        "bit_size < 32" in mem_cb and ".align = 4," in mem_cb,
    )
    check(
        "no 8/16-bit access size is requested",
        "bit_size = 8;" not in mem_cb and "bit_size = 16;" not in mem_cb,
    )
    check(
        "the atomics fallback for narrow stores is enabled",
        ".may_lower_unaligned_stores_to_atomics = true" in pco_nir,
        "a narrow store has no encoding; the pass rewrites it as iand/ior atomics",
    )

    # 4. The backend really can translate the pieces that fallback needs.
    check(
        "iand/ior map to the ISA atomic opcodes",
        "PCO_ATOM_OP_AND" in trans_nir and "PCO_ATOM_OP_OR" in trans_nir,
    )
    check(
        "32-bit buffer atomics are translated",
        "nir_intrinsic_ssbo_atomic" in trans_nir
        and "trans_global_atomic_buffer" in trans_nir,
    )
    check(
        "buffer stores are translated for every stage",
        "trans_store_buffer(" in trans_nir,
        "vertexPipelineStoresAndAtomics rests on stores not being stage-gated",
    )

    # 5. The 8/16-bit values that come out of storage can be computed with.
    check(
        "8-bit ALU is widened",
        "alu->def.bit_size == 8" in read(PCO_DIR, "pco_nir.c"),
    )
    cb_start = pco_nir.find("pco_lower_bit_size_cb(const nir_instr")
    alu_cb = (
        pco_nir[cb_start : pco_nir.find("\n}\n", cb_start)] if cb_start != -1 else ""
    )
    check("16-bit ALU is widened", "alu->def.bit_size == 16" in alu_cb)
    check(
        "16-bit comparisons are widened through their operands",
        "alu->def.bit_size == 1" in alu_cb
        and "src->bit_size == 8 || src->bit_size == 16" in alu_cb,
    )

    # 6. The features that need paths that do not exist stay off, and nothing
    #    the emulator requires is advertised as false.
    for feature in (
        "storagePushConstant16",
        "storagePushConstant8",
        "storageInputOutput16",
    ):
        check(f"{feature} stays off", f".{feature} = false," in physical)

    for feature in (
        "storageBuffer8BitAccess",
        "uniformAndStorageBuffer8BitAccess",
        "storageBuffer16BitAccess",
        "uniformAndStorageBuffer16BitAccess",
        "vertexPipelineStoresAndAtomics",
        "independentBlend",
    ):
        check(
            f"{feature} is not advertised as false",
            f".{feature} = false," not in physical,
        )

    failed = 0
    for name, ok, detail in checks:
        print(f"  {'PASS' if ok else 'FAIL'}: {name}")
        if not ok:
            if detail:
                print(f"        {detail}")
            failed += 1

    print(f"\nCheckpoint: {len(checks) - failed}/{len(checks)} checks passed")
    if failed:
        print("  ❌ the emulator would reject this device")
        return 1
    print("  ✅ every feature Eden/Yuzu require is advertised and lowered")
    return 0


if __name__ == "__main__":
    sys.exit(main())
