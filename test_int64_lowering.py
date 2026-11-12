#!/usr/bin/env python3
"""
M20 64-bit integer (shaderInt64) validation gate.

The USC has no 64-bit ALU and no 64-bit memory instruction, so `shaderInt64`
is only honest when the compiler lowers both halves of the problem:

1. **ALU** — `nir_lower_int64` must actually do something. It is a no-op unless
   `lower_int64_options` is set, so the options block has to enable it (and the
   pack/unpack helpers that come with the 32-bit-pair representation).
2. **Memory** — a 64-bit load or store must be split into 32-bit accesses by
   `nir_lower_mem_access_bit_sizes` with a callback that caps the width, and it
   must run *before* `nir_lower_int64` so the halves are already 32-bit.
3. **Advertisement** — `.shaderInt64 = true` with `.shaderFloat64` still false
   (there are no double-precision units), and no 64-bit atomic features, which
   would need native 64-bit atomics in memory.
4. **Evidence** — the vendor driver reports `shaderInt64 = 1.0` on this device,
   so emulation is what the silicon expects, not a feature the hardware lacks.

Host-only: this checks compiler wiring, not generated code. No GPU is used.
"""

import os
import re
import sys

MESA_SRC = "/home/ameya/bxm-research/research/mesa-src"
PCO_NIR_C = os.path.join(MESA_SRC, "src/imagination/pco/pco_nir.c")
PVR_PHYSICAL_DEVICE_C = os.path.join(
    MESA_SRC, "src/imagination/vulkan/pvr_physical_device.c"
)
VKJSON = "/home/ameya/bxm-research/device-info/vkjson.json"

PASS = 0
FAIL = 0


def check(condition, desc):
    global PASS, FAIL
    if condition:
        PASS += 1
        print(f"  PASS: {desc}")
    else:
        FAIL += 1
        print(f"  FAIL: {desc}")


def read(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return f.read()
    except FileNotFoundError:
        return ""


pco = read(PCO_NIR_C)
phys = read(PVR_PHYSICAL_DEVICE_C)
vkjson = read(VKJSON)

print("=" * 64)
print("M20 64-bit integer (shaderInt64) validation")
print("=" * 64)

# ---------------------------------------------------------------------------
# 1. ALU lowering is enabled (not merely called)
# ---------------------------------------------------------------------------
print("\n[1] 64-bit ALU lowering is enabled")
check(bool(re.search(r"\.lower_int64_options\s*=\s*~0U\s*,", pco)),
      "lower_int64_options = ~0U (nir_lower_int64 lowers every 64-bit op)")
check(bool(re.search(r"\.lower_pack_64_2x32\s*=\s*true\s*,", pco)),
      "lower_pack_64_2x32 = true")
check(bool(re.search(r"\.lower_pack_64_2x32_split\s*=\s*true\s*,", pco)),
      "lower_pack_64_2x32_split = true")
check(bool(re.search(r"\.lower_unpack_64_2x32_split\s*=\s*true\s*,", pco)),
      "lower_unpack_64_2x32_split = true")
check("NIR_PASS(progress, nir, nir_lower_int64);" in pco,
      "nir_lower_int64 runs in the PCO optimization loop")

# ---------------------------------------------------------------------------
# 2. 64-bit memory accesses are split into 32-bit ones
# ---------------------------------------------------------------------------
print("\n[2] 64-bit memory access splitting")
check("pco_mem_access_size_align" in pco,
      "pco_mem_access_size_align() callback exists")
check(bool(re.search(
          r"if\s*\(\s*bit_size\s*>\s*32\s*\)\s*\n\s*bit_size\s*=\s*32;", pco)),
      "callback caps the access width at 32 bits")

modes_match = re.search(
    r"\.modes\s*=\s*([^,]*nir_var_mem_ubo[^;]*);", pco, re.DOTALL
)
modes_text = modes_match.group(1) if modes_match else ""
for mode in ("nir_var_mem_ubo", "nir_var_mem_ssbo", "nir_var_mem_global",
             "nir_var_mem_constant", "nir_var_mem_push_const",
             "nir_var_mem_shared"):
    check(mode in modes_text, f"mem access lowering covers {mode}")

check(".callback = pco_mem_access_size_align," in pco,
      "mem access lowering uses the 32-bit-capping callback")

# The split must happen before the ALU lowering, so the halves of a 64-bit
# load are already 32-bit when nir_lower_int64 runs.
split_at = pco.find("nir_lower_mem_access_bit_sizes, &mem_bit_sizes")
int64_at = pco.find("NIR_PASS(progress, nir, nir_lower_int64);")
check(split_at != -1 and int64_at != -1 and split_at < int64_at,
      "mem access splitting is ordered before nir_lower_int64")

# The pass must be inside the optimization loop, not in dead code.
opt_at = pco.find("static void pco_nir_opt(")
translate_at = pco.find("void pco_preprocess_nir(")
check(opt_at != -1 and split_at > opt_at and
      (translate_at == -1 or split_at < translate_at),
      "mem access splitting runs inside pco_nir_opt()")

# ---------------------------------------------------------------------------
# 3. Advertisement is consistent with what the compiler does
# ---------------------------------------------------------------------------
print("\n[3] Feature advertisement")
check(bool(re.search(r"\.shaderInt64\s*=\s*true\s*,", phys)),
      "shaderInt64 = true")
check(bool(re.search(r"\.shaderFloat64\s*=\s*false\s*,", phys)),
      "shaderFloat64 stays false (no double-precision units)")
check(bool(re.search(r"\.shaderBufferInt64Atomics\s*=\s*false\s*,", phys)),
      "shaderBufferInt64Atomics stays false (no native 64-bit atomics)")
check(bool(re.search(r"\.shaderSharedInt64Atomics\s*=\s*false\s*,", phys)),
      "shaderSharedInt64Atomics stays false")

# ---------------------------------------------------------------------------
# 4. Vendor evidence: the silicon runs 64-bit integer emulation
# ---------------------------------------------------------------------------
print("\n[4] Vendor device evidence")
m = re.search(r'"shaderInt64"\s*:\s*([\d.]+)', vkjson)
vendor_int64 = m.group(1) if m else None

check(vendor_int64 == "1.0",
      f"vendor driver reports shaderInt64 = 1.0 (got {vendor_int64})")

# ---------------------------------------------------------------------------
print("\n" + "=" * 64)
print(f"Results: {PASS} passed, {FAIL} failed")
print("=" * 64)

if FAIL > 0:
    print("\n>>> M20 INT64: SOME CHECKS FAILED <<<")
    sys.exit(1)

print("\n>>> M20 INT64: ALL CHECKS PASSED <<<")
sys.exit(0)
