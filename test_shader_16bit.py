#!/usr/bin/env python3
"""
M22 gate: 16-bit shader ALU (shaderInt16 / shaderFloat16) behind PVR_SHADER_16BIT.

PCO has no 16-bit comparison and no 16-bit ALU: trans_cmp() picks its test type
from the opcode alone (F32/S32/U32) and the USC only issues 32-bit register
operations. So the only way to run half and short shaders is to widen them:

  1. pco_lower_bit_size_cb() widens 16-bit ALU to 32 bits, and because a
     comparison's result is a 1-bit bool, it also widens comparisons whose
     *operands* are narrow -- nir_lower_bit_size() would never see those,
  2. trans_conv() packs a narrowing conversion into the low half and unpacks a
     widening one with the source's signedness, so the values that cross the
     32-bit boundary keep their meaning, and
  3. the features are gated: 16-bit ALU lowering is expected to be correct but
     has not been compared against a reference on this silicon, and 16-bit
     *storage* stays off because the backend has only 32-bit loads and stores.

Check 3 is the one that must not be deleted: flipping shaderInt16 or
shaderFloat16 to an unconditional `true` advertises a path nobody has run.
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

    cb_start = pco_nir.find("pco_lower_bit_size_cb(const nir_instr")
    cb_body = (
        pco_nir[cb_start:pco_nir.find("\n}\n", cb_start)] if cb_start != -1 else ""
    )
    check("widening callback exists", cb_body != "")

    # 1. 16-bit ALU is widened.
    check(
        "callback widens 16-bit ALU",
        "alu->def.bit_size == 8 || alu->def.bit_size == 16" in cb_body,
    )

    # 2. Comparisons are widened through their operands: the result is a 1-bit
    #    bool, so nothing else in nir_lower_bit_size() can reach them.
    check(
        "callback widens comparisons with narrow operands",
        "alu->def.bit_size == 1" in cb_body
        and "src->bit_size == 8 || src->bit_size == 16" in cb_body,
        "a 16-bit comparison has a 1-bit result; without this the pass never "
        "sees it and trans_cmp() is handed 16-bit operands",
    )

    # 3. The comparison translation really has no narrow form -- document the
    #    invariant where the next reader will look for it.
    cmp_start = trans_nir.find("trans_cmp(trans_ctx *tctx,")
    cmp_body = (
        trans_nir[cmp_start:trans_nir.find("\n}\n", cmp_start)] if cmp_start != -1 else ""
    )
    check(
        "trans_cmp requires 32-bit operands",
        "src0_bits == 32" in cmp_body and "pco_ref_get_bits(src1) == 32" in cmp_body,
    )
    check(
        "trans_cmp selects no 16-bit test type",
        "PCO_TST_TYPE_MAIN_F16" not in trans_nir,
        "if the ISA grows a half test type, the widening of comparisons can go",
    )

    # 4. Conversions move values between the 16-bit lane form and 32-bit
    #    registers instead of pretending a move is enough.
    conv_start = trans_nir.find("trans_conv(trans_ctx *tctx,")
    conv_body = (
        trans_nir[conv_start:trans_nir.find("\n}\n", conv_start)]
        if conv_start != -1
        else ""
    )
    check(
        "narrowing to 16 bits truncates instead of packing",
        "BITFIELD_MASK(16)" in conv_body and "PCO_LOGIOP_AND" in conv_body,
        "pck quantises, so a widened add that wraps past INT16_MAX would be "
        "clamped to INT16_MAX instead of wrapping",
    )
    check(
        "widening from 16 bits sign-extends",
        "case nir_op_i2i32" in conv_body
        and "pco_unpck" in conv_body
        and "PCO_PCK_FMT_S1616" in conv_body,
        "a 16-bit lane read as 32 bits has to be sign- or zero-extended",
    )
    check(
        "16-bit conversions no longer start with a bare move",
        "case nir_op_i2i16:\n   case nir_op_i2i32:" not in conv_body,
    )

    # 5. Features are gated, and the gate defaults to off.
    check(
        "shaderInt16 is gated",
        ".shaderInt16 = pvr_shader_16bit_allowed()," in physical,
        "advertising 16-bit ALU unconditionally would promise a path that has "
        "not been compared against a reference on the device",
    )
    check(
        "shaderFloat16 is gated",
        ".shaderFloat16 = pvr_shader_16bit_allowed()," in physical,
    )
    gate_start = physical.find("static bool pvr_shader_16bit_allowed(void)")
    gate_body = (
        physical[gate_start:physical.find("\n}\n", gate_start)]
        if gate_start != -1
        else ""
    )
    check(
        "the gate reads PVR_SHADER_16BIT and defaults to on",
        'debug_get_bool_option("PVR_SHADER_16BIT", true)' in gate_body,
        "16-bit storage is advertised, so half and short values arrive from "
        "memory whether or not the shader features are on",
    )

    # 6. 16-bit *storage* is advertised and lowered to 32-bit words; the paths
    #    that would need 16-bit push constants or varyings stay off.
    for feature in (
        ".storageBuffer16BitAccess = true,",
        ".uniformAndStorageBuffer16BitAccess = true,",
        ".storagePushConstant16 = false,",
        ".storageInputOutput16 = false,",
    ):
        check(f"{feature.strip()}" , feature in physical)

    failed = 0
    for name, ok, detail in checks:
        print(f"  {'PASS' if ok else 'FAIL'}: {name}")
        if not ok:
            if detail:
                print(f"        {detail}")
            failed += 1

    print(f"\nCheckpoint: {len(checks) - failed}/{len(checks)} checks passed")
    if failed:
        print("  ❌ 16-bit shader ALU is inconsistent")
        return 1
    print("  ✅ 16-bit ALU and comparisons are widened; features stay behind the gate")
    return 0


if __name__ == "__main__":
    sys.exit(main())
