#!/usr/bin/env python3
"""
M20 (8-bit) gate: shaderInt8 widening, and 8-bit storage staying off.

The USC has no 8-bit ALU, so shaderInt8 is implemented by widening 8-bit ALU
operations to 32 bits before instruction selection. 8-bit *storage* is a
different problem: the PCO backend only has 32-bit buffer loads and stores, so
advertising storageBuffer8BitAccess would abort compilation.

The gate therefore enforces two things that must stay true together:

  1. the widening pass exists and runs before the memory-access pass, and
  2. 8-bit storage is not advertised unless the backend grew byte access.

The second check is the important one: it fails the moment somebody flips the
feature on without touching trans_load_buffer()/trans_store_buffer().

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
        and "alu->def.bit_size != 8" in pco_nir,
    )
    cb_start = pco_nir.find("pco_lower_bit_size_cb(const nir_instr")
    cb_body = (
        pco_nir[cb_start:pco_nir.find("\n}\n", cb_start)] if cb_start != -1 else ""
    )
    check("callback widens 8-bit to 32-bit", "return 32;" in cb_body)
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

    # 3. Feature state.
    check("shaderInt8 is advertised", ".shaderInt8 = true," in physical)
    check(
        "storageBuffer8BitAccess stays off",
        ".storageBuffer8BitAccess = false," in physical,
    )
    check(
        "uniformAndStorageBuffer8BitAccess stays off",
        ".uniformAndStorageBuffer8BitAccess = false," in physical,
    )
    check("storagePushConstant8 stays off", ".storagePushConstant8 = false," in physical)

    # 4. The backend really is 32-bit only. If byte access ever lands, this
    #    check stops being a reason to keep the features off -- so make the
    #    gate require both halves to agree either way.
    backend_32bit_only = (
        "assert(bits == 32);" in trans_nir
        and "case 32:" in trans_nir
        and 'UNREACHABLE("");' in trans_nir
    )
    storage_off = ".storageBuffer8BitAccess = false," in physical

    check(
        "backend and advertised 8-bit storage agree",
        storage_off or not backend_32bit_only,
        "8-bit storage is advertised but trans_load_buffer/trans_store_buffer "
        "still only handle 32-bit accesses",
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
    print("  ✅ int8 is widened to 32-bit; 8-bit storage stays off with the backend")
    return 0


if __name__ == "__main__":
    sys.exit(main())
