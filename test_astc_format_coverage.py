#!/usr/bin/env python3
"""
L2.5 CI Test: ASTC LDR format coverage for the BXM-8-256 (M17) path.

Validates that the three tables that must agree for native ASTC to work actually
agree, without touching hardware:

  1. mesa's Vulkan -> pipe format mapping (src/vulkan/util/vk_format.c)
  2. the PVR texstate format table     (pvr_arch_formats.c)
  3. the PVR compressed border table   (pvr_arch_border.c)
  4. the generated ROGUE_TEXSTATE_FORMAT_COMPRESSED_* enum values
  5. the advertised Vulkan feature flag and the BXM device profile

Host-bound only - no GPU required.
"""

import os
import re
import sys

MESA_SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "research", "mesa-src")
VK_FORMAT_C = os.path.join(MESA_SRC, "src/vulkan/util/vk_format.c")
PVR_FORMATS_C = os.path.join(MESA_SRC, "src/imagination/vulkan/pvr_arch_formats.c")
PVR_BORDER_C = os.path.join(MESA_SRC, "src/imagination/vulkan/pvr_arch_border.c")
PVR_PHYSDEV_C = os.path.join(MESA_SRC, "src/imagination/vulkan/pvr_physical_device.c")
BXM_DEVICE_H = os.path.join(MESA_SRC, "src/imagination/common/device_info/bxm-8-256.h")
TEXSTATE_H = os.path.join(
    MESA_SRC, "build-android/src/imagination/csbgen/rogue/texstate.h"
)
TEXSTATE_XML = os.path.join(MESA_SRC, "src/imagination/csbgen/rogue/texstate.xml")

# The 14 ASTC LDR block sizes Vulkan requires when textureCompressionASTC_LDR
# is advertised.
ASTC_BLOCK_SIZES = [
    "4x4", "5x4", "5x5", "6x5", "6x6", "8x5",
    "8x6", "8x8", "10x5", "10x6", "10x8", "10x10",
    "12x10", "12x12",
]

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


def texstate_enum_name(pipe_format):
    """PIPE_FORMAT_ASTC_4x4[_SRGB] -> ASTC_4X4 (the texstate enum suffix).

    The hardware has a single ASTC format index per block size: sRGB decoding is
    selected by the image-word0 gamma bit, exactly as for the ETC2/EAC formats.
    """
    name = pipe_format[len("PIPE_FORMAT_"):].upper()
    if name.endswith("_SRGB"):
        name = name[: -len("_SRGB")]
    return name


def main():
    print("[*] M17 ASTC LDR coverage validation")
    print("    mesa source: %s" % MESA_SRC)

    # --- 1. Vulkan format -> pipe format mapping -------------------------------
    vk_format_src = read(VK_FORMAT_C)
    vk_to_pipe = dict(
        re.findall(
            r"\[(VK_FORMAT_ASTC_[0-9x]+_(?:UNORM|SRGB)_BLOCK)\]\s*=\s*(PIPE_FORMAT_ASTC_[0-9xA-Za-z_]*)",
            vk_format_src,
        )
    )

    expected_vk = []
    for size in ASTC_BLOCK_SIZES:
        expected_vk.append("VK_FORMAT_ASTC_%s_UNORM_BLOCK" % size)
        expected_vk.append("VK_FORMAT_ASTC_%s_SRGB_BLOCK" % size)

    missing_vk = [fmt for fmt in expected_vk if fmt not in vk_to_pipe]
    check(not missing_vk, "vk_format.c missing ASTC mappings: %s" % ", ".join(missing_vk))
    check(
        len(vk_to_pipe) == len(expected_vk),
        "vk_format.c maps %d ASTC LDR formats, expected %d"
        % (len(vk_to_pipe), len(expected_vk)),
    )
    print("[+] vk_format.c maps %d ASTC LDR formats" % len(vk_to_pipe))

    # --- 2. PVR texstate format table ----------------------------------------
    formats_src = read(PVR_FORMATS_C)
    compressed_entries = {
        pipe_format: texstate_fmt
        for pipe_format, texstate_fmt in re.findall(
            r"FORMAT_COMPRESSED\(\s*([A-Za-z0-9_]+)\s*,\s*([A-Za-z0-9_]+)\s*\)",
            formats_src,
        )
        # Skip the FORMAT_COMPRESSED(vk, tex_fmt) macro definition itself.
        if pipe_format != "vk"
    }

    for pipe_format in sorted(set(vk_to_pipe.values())):
        # Table keys drop the PIPE_FORMAT_ prefix because the FORMAT_COMPRESSED
        # macro pastes it back on.
        entry_token = pipe_format[len("PIPE_FORMAT_"):]
        entry = compressed_entries.get(entry_token)
        expected_enum = texstate_enum_name(pipe_format)
        if not check(
            entry is not None,
            "pvr_arch_formats.c has no FORMAT_COMPRESSED entry for %s" % pipe_format,
        ):
            continue
        check(
            entry == expected_enum,
            "%s maps to texstate format %s, expected %s"
            % (pipe_format, entry, expected_enum),
        )

    for texstate_fmt in compressed_entries.values():
        check(
            texstate_fmt.startswith("ASTC_") or texstate_fmt.startswith("ETC2")
            or texstate_fmt.startswith("EAC_"),
            "unexpected compressed texstate format token: %s" % texstate_fmt,
        )

    astc_fmt_entries = [v for v in compressed_entries.values() if v.startswith("ASTC_")]
    check(
        len(astc_fmt_entries) == len(expected_vk),
        "pvr_arch_formats.c has %d ASTC entries, expected %d"
        % (len(astc_fmt_entries), len(expected_vk)),
    )
    print("[+] pvr_arch_formats.c has %d ASTC entries" % len(astc_fmt_entries))

    # --- 3. Compressed border colour table -----------------------------------
    border_src = read(PVR_BORDER_C)
    border_block = border_src.split("pvr_tex_format_compressed_table", 1)
    border_entries = {}
    if len(border_block) == 2:
        table_body = border_block[1].split("};", 1)[0]
        border_entries = dict(
            re.findall(
                r"FORMAT\(\s*([A-Za-z0-9_]+)\s*,\s*[A-Za-z0-9_]+\s*,\s*([A-Za-z0-9_]+)\s*\)",
                table_body,
            )
        )

    for size in ASTC_BLOCK_SIZES:
        enum_suffix = "ASTC_%s" % size.upper()
        simple = border_entries.get(enum_suffix)
        if not check(
            simple is not None,
            "pvr_arch_border.c has no compressed border entry for %s" % enum_suffix,
        ):
            continue
        check(
            simple == "U8U8U8U8",
            "%s border entry uses simple format %s, expected U8U8U8U8"
            % (enum_suffix, simple),
        )
    astc_border_entries = [k for k in border_entries if k.startswith("ASTC_")]
    print("[+] pvr_arch_border.c covers %d ASTC border formats" % len(astc_border_entries))

    # --- 4. Generated texstate enum values -----------------------------------
    texstate_src = read(TEXSTATE_H) or read(TEXSTATE_XML)
    for index, size in enumerate(ASTC_BLOCK_SIZES):
        enum_name = "ROGUE_TEXSTATE_FORMAT_COMPRESSED_ASTC_%s" % size.upper()
        value_match = re.search(
            r"%s\s*(?:=\s*|\<value name=\"ASTC_%s\"\s+value=)(\"?)(\d+)" % (enum_name, size),
            texstate_src,
        )
        if not check(
            value_match is not None,
            "texstate enum %s (hardware format index %d) not found" % (enum_name, index),
        ):
            continue
        check(
            int(value_match.group(2)) == index,
            "%s has hardware index %s, expected %d"
            % (enum_name, value_match.group(2), index),
        )
    print("[+] texstate ASTC hardware format indices 0-13 verified")

    # --- 5. Feature advertisement --------------------------------------------
    physdev_src = read(PVR_PHYSDEV_C)
    check(
        ".textureCompressionASTC_LDR = PVR_HAS_FEATURE(dev_info, astc)" in physdev_src,
        "textureCompressionASTC_LDR is not gated on the device has_astc feature",
    )
    check(
        re.search(r"textureCompressionASTC_LDR\s*=\s*false", physdev_src) is None,
        "textureCompressionASTC_LDR is still hard-disabled",
    )

    bxm_src = read(BXM_DEVICE_H)
    check(
        re.search(r"\.has_astc\s*=\s*true", bxm_src) is not None,
        "bxm-8-256.h does not declare .has_astc = true",
    )
    print("[+] ASTC gated on has_astc, BXM-8-256 profile sets has_astc = true")

    # --- Result --------------------------------------------------------------
    print("")
    print("Checks run: %d" % checks)
    if failures:
        print("FAILURES (%d):" % len(failures))
        for failure in failures:
            print("  [FAIL] %s" % failure)
        return 1

    print(">>> ALL ASTC (M17) COVERAGE CHECKS PASSED <<<")
    return 0


if __name__ == "__main__":
    sys.exit(main())
