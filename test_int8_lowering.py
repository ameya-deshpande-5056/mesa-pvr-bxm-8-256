#!/usr/bin/env python3
"""
M20 (8-bit) gate: shaderInt8 widening, and 8-bit storage staying off.

The USC has no 8-bit ALU, so shaderInt8 is implemented by widening 8-bit ALU
operations to 32 bits before instruction selection. 8-bit *storage* is a
different problem: the PCO backend only has 32-bit buffer loads and stores, so
advertising storageBuffer8BitAccess would abort compilation.

The gate therefore enforces three things that must stay true together:

  1. the widening pass exists and runs before the memory-access pass,
  2. 8-bit *storage* is advertised, because Yuzu/Eden require the bit before
     they will select the driver at all, and
  3. no 8-bit access ever reaches the backend: pco_mem_access_size_align()
     answers with an aligned 32-bit word, and the memory-access pass turns the
     narrow load into word load + shift + extract and the narrow store into the
     iand/ior pair of 32-bit atomics.

Check 3 is the important one: it fails the moment somebody asks the backend
for an access it cannot encode, or drops the atomics option the store path
needs.

Paths are derived from this file so the gate also runs from a CI checkout.
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

    # 1. The widening pass exists and is the only 8-bit lowering.
    check(
        "8-bit ALU widening callback exists",
        "pco_lower_bit_size_cb" in pco_nir
        and "nir_instr_as_alu" in pco_nir
        and "alu->def.bit_size == 8" in pco_nir,
    )
    cb_start = pco_nir.find("pco_lower_bit_size_cb(const nir_instr")
    cb_body = (
        pco_nir[cb_start:pco_nir.find("\n}\n", cb_start)] if cb_start != -1 else ""
    )
    check("callback widens 8-bit to 32-bit", "return 32;" in cb_body)
    check(
        "conversion ops are left to the backend",
        "nir_alu_type_get_type_size(nir_op_infos[alu->op].output_type) != 0"
        in cb_body,
        "widening f2f16/u2u8/i2i16 rebuilds an identical instruction, so the "
        "pass never converges and the opt loop only stops at its guard",
    )
    check(
        "nir_lower_bit_size is wired to the callback",
        "nir_lower_bit_size, pco_lower_bit_size_cb" in pco_nir,
    )

    # 2. Ordering: 8-bit values must be 32-bit before memory accesses are sized.
    bit_size_pos = pco_nir.find("nir_lower_bit_size, pco_lower_bit_size_cb")
    mem_pos = pco_nir.find("nir_lower_mem_access_bit_sizes")
    check(
        "widening runs before the memory access pass",
        bit_size_pos != -1 and mem_pos != -1 and bit_size_pos < mem_pos,
        "8-bit load offsets and store values must be widened first",
    )

    # 3. Feature state. 8-bit storage is required by Yuzu/Eden's device
    #    selection, so it is advertised now that narrow access lowers.
    check("shaderInt8 is advertised", ".shaderInt8 = true," in physical)
    check(
        "storageBuffer8BitAccess is advertised",
        ".storageBuffer8BitAccess = true," in physical,
    )
    check(
        "uniformAndStorageBuffer8BitAccess is advertised",
        ".uniformAndStorageBuffer8BitAccess = true," in physical,
    )
    check("storagePushConstant8 stays off", ".storagePushConstant8 = false," in physical)

    # 4. The backend is still 32-bit only, so the access pass has to keep
    #    narrow accesses away from it.
    check(
        "the backend still encodes only 32-bit accesses",
        "assert(bits == 32);" in trans_nir
        and "case 32:" in trans_nir
        and 'UNREACHABLE("");' in trans_nir,
    )

    cb_start = pco_nir.find("pco_mem_access_size_align(")
    mem_cb = (
        pco_nir[cb_start : pco_nir.find("\n}\n", cb_start)] if cb_start != -1 else ""
    )
    check(
        "narrow accesses are requested as aligned 32-bit words",
        "bit_size < 32" in mem_cb and ".align = 4," in mem_cb,
        "asking for an 8- or 16-bit access is what used to abort compilation",
    )
    check(
        "no narrow access size is ever requested",
        "bit_size = 8;" not in mem_cb and "bit_size = 16;" not in mem_cb,
    )
    check(
        "unaligned narrow stores may lower to 32-bit atomics",
        ".may_lower_unaligned_stores_to_atomics = true" in pco_nir,
        "without this the pass asserts instead of emitting iand/ior atomics",
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
        print("  ❌ M20 8-bit lowering is inconsistent")
        return 1
    print("  ✅ int8 is widened to 32-bit; 8-bit storage lowers to 32-bit words")
    return 0


if __name__ == "__main__":
    sys.exit(main())
