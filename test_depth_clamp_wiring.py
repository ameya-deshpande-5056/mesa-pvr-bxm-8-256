#!/usr/bin/env python3
"""
L2.6 CI Test: ISP depth clamp wiring (M18).

Depth clamping is a two-bit contract on the BXM-8-256: the Tile Accelerator has
to stop clipping Z (TA_STATE_PPP_CTRL.clip_mode) while the Image Synthesis
Processor clamps the Z it writes into the vertex data
(TA_OUTPUT_SEL.isp_position_depth_clamp_z). If one bit is set without the other
the symptom is silent: fragments keep an out-of-range depth or get discarded,
with no API error.

This test keeps the three pieces that must agree in sync:
  1. the advertised Vulkan feature (and the vendor evidence for it),
  2. the shared clamp predicate and its users,
  3. the generated packet fields/bit positions it relies on.

Host-bound only - no GPU required.
"""

import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
MESA_SRC = os.path.join(REPO_ROOT, "research", "mesa-src")
VKJSON = os.path.join(REPO_ROOT, "device-info", "vkjson.json")
CMD_BUFFER_C = os.path.join(MESA_SRC, "src/imagination/vulkan/pvr_arch_cmd_buffer.c")
PHYSDEV_C = os.path.join(MESA_SRC, "src/imagination/vulkan/pvr_physical_device.c")
PPP_H = os.path.join(MESA_SRC, "build-android/src/imagination/csbgen/rogue/ppp.h")
PPP_XML = os.path.join(MESA_SRC, "src/imagination/csbgen/rogue/ppp.xml")

failures = []
checks = 0


def check(condition, message):
    global checks
    checks += 1
    if not condition:
        failures.append(message)
    return condition


def read(path):
    if not os.path.isfile(path):
        failures.append("missing file: %s" % path)
        return ""
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return handle.read()


def main():
    global checks, failures

    print("[*] M18 ISP depth clamp validation")

    physdev_src = read(PHYSDEV_C)
    cmd_src = read(CMD_BUFFER_C)
    ppp_src = read(PPP_H) or read(PPP_XML)

    # --- 1. Advertised feature + vendor evidence -----------------------------
    check(
        re.search(r"\.depthClamp\s*=\s*true", physdev_src) is not None,
        "pvr_physical_device.c still advertises depthClamp = false",
    )

    import json

    try:
        with open(VKJSON, "r", encoding="utf-8") as handle:
            vkjson = json.load(handle)
        vendor_depth_clamp = vkjson["devices"][0]["features"].get("depthClamp")
    except Exception as error:  # noqa: BLE001 - report any parse failure as a check
        vendor_depth_clamp = None
        failures.append("could not read vendor depthClamp evidence: %s" % error)
        checks += 1

    check(
        vendor_depth_clamp == 1.0,
        "vendor vkjson does not report depthClamp = 1.0 (got %r)" % (vendor_depth_clamp,),
    )
    print("[+] depthClamp advertised; vendor driver reports depthClamp = 1.0")

    # --- 2. Shared predicate -------------------------------------------------
    predicate = re.search(
        r"static bool\s+pvr_depth_clamp_enabled\((.*?)\n\}", cmd_src, re.S
    )
    if check(predicate is not None, "pvr_depth_clamp_enabled() predicate is missing"):
        body = predicate.group(1)
        for mode in (
            "VK_MESA_DEPTH_CLIP_ENABLE_FALSE",
            "VK_MESA_DEPTH_CLIP_ENABLE_TRUE",
            "VK_MESA_DEPTH_CLIP_ENABLE_NOT_CLAMP",
        ):
            check(
                mode in body,
                "pvr_depth_clamp_enabled() does not handle %s" % mode,
            )
        check(
            "depth_clamp_enable" in body,
            "pvr_depth_clamp_enabled() ignores the core depthClampEnable state",
        )

    # --- 3. Both consumers drive from the predicate --------------------------
    check(
        re.search(
            r"state\.isp_position_depth_clamp_z\s*=\s*\n?\s*pvr_depth_clamp_enabled\(",
            cmd_src,
        )
        is not None,
        "TA_OUTPUT_SEL.isp_position_depth_clamp_z is not derived from the clamp predicate",
    )
    check(
        re.search(
            r"control\.clip_mode\s*=\s*pvr_depth_clamp_enabled\(dynamic_state\)\s*\n?\s*\?\s*ROGUE_TA_CLIP_MODE_NO_FRONT_OR_REAR\s*\n?\s*:\s*ROGUE_TA_CLIP_MODE_FRONT_REAR",
            cmd_src,
        )
        is not None,
        "TA clip mode is not derived from the same clamp predicate",
    )
    check(
        "isp_position_depth_clamp_z = true;" not in cmd_src,
        "isp_position_depth_clamp_z is hardcoded on instead of tracking the draw state",
    )

    # --- 4. Dynamic-state plumbing ------------------------------------------
    update_required = cmd_src.split("pvr_ppp_state_update_required", 1)
    if check(len(update_required) == 2, "pvr_ppp_state_update_required() is missing"):
        body = update_required[1].split("\n}", 1)[0]
        check(
            "MESA_VK_DYNAMIC_RS_DEPTH_CLAMP_ENABLE" in body,
            "PPP state update is not triggered by depthClampEnable changes",
        )
        check(
            "MESA_VK_DYNAMIC_RS_DEPTH_CLIP_ENABLE" in body,
            "PPP state update is not triggered by depthClipEnable changes",
        )

    emit_block = cmd_src.split("pvr_emit_dirty_ppp_state", 1)
    if check(len(emit_block) == 2, "pvr_emit_dirty_ppp_state() is missing"):
        body = emit_block[1]
        check(
            "pvr_setup_output_select(cmd_buffer);" in body,
            "output selects are never re-emitted after a pipeline bind",
        )
        # The dynamic-only path must re-emit the output selects too, otherwise a
        # depthClampEnable change without a pipeline rebind leaves the ISP clamp
        # bit stale.
        dynamic_path = body.split("} else if (", 1)
        check(
            len(dynamic_path) == 2
            and "pvr_setup_output_select(cmd_buffer);" in dynamic_path[1].split("}", 1)[0],
            "dynamic depth clamp changes do not re-emit TA output selects",
        )

    # --- 5. Generated packet fields -----------------------------------------
    check(
        "isp_position_depth_clamp_z" in ppp_src,
        "TA_OUTPUT_SEL has no isp_position_depth_clamp_z field",
    )
    pack = re.search(
        r"__pvr_uint\(values->isp_position_depth_clamp_z,\s*(\d+),\s*(\d+)\)", ppp_src
    )
    check(
        pack is not None and pack.group(1) == "17" and pack.group(2) == "17",
        "isp_position_depth_clamp_z is not packed at bit 17 of TA_OUTPUT_SEL",
    )

    clip_mode_bit = re.search(
        r"__pvr_uint\(values->clip_mode,\s*(\d+),\s*(\d+)\)", ppp_src
    )
    check(
        clip_mode_bit is not None
        and clip_mode_bit.group(1) == "10"
        and clip_mode_bit.group(2) == "11",
        "clip_mode is not packed at bits 10:11 of TA_STATE_PPP_CTRL",
    )

    # --- Result --------------------------------------------------------------
    print("")
    print("Checks run: %d" % checks)
    if failures:
        print("FAILURES (%d):" % len(failures))
        for failure in failures:
            print("  [FAIL] %s" % failure)
        return 1

    print(">>> ALL DEPTH CLAMP (M18) WIRING CHECKS PASSED <<<")
    return 0


if __name__ == "__main__":
    sys.exit(main())
