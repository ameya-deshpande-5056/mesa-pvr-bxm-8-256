#!/usr/bin/env python3
"""
Level 2 CI Test: Header Content Validation for Volcanic (BXM-8-256) CSBgen Generated Headers.
Validates that all 10 generated C headers have expected structure, enums, structs, and pack/unpack functions.
Host-bound only - no GPU required.
"""

import os
import sys

# Paths
MESA_SRC = "/home/ameya/bxm-research/research/mesa-src"
VOLCANIC_HWDEF_DIR = os.path.join(MESA_SRC, "src/imagination/include/hwdef")

# Header content checks: header_filename -> list of required patterns (relaxed matching)
# These patterns confirm the essential structure exists (enums, structs, pack/unpack, bitfields)
HEADER_CHECKS = {
    "cdm.h": [
        "VOLCANIC_CDMCTRL_BLOCK_TYPE",  # enum block types
        "COMPUTE_KERNEL",               # enum value
        "STREAM_LINK",                  # enum value
        "STREAM_TERMINATE",             # enum value
        "VOLCANIC_CDMCTRL",             # struct name
        "pack",                         # pack function
        "unpack",                       # unpack function
        "to_str",                       # string conversion
        "BLOCK_TYPE",                   # type marker
    ],
    "cr.h": [
        "VOLCANIC_CR",                  # struct/enum prefix
        "VOLCANIC_CR_CR_REG_WORD0",     # specific struct from the CR register block
        "pack",                         # pack function
        "unpack",                       # unpack function
        "header",                       # length/header macro suffix
        "length",                       # length define
    ],
    "ipf.h": [
        "VOLCANIC_IPF",                 # struct/enum prefix
        "pack",
        "unpack",
        "length",
        "header",
    ],
    "kmd_stream.h": [
        "VOLCANIC_KMD_STREAM",
        "pack",
        "unpack",
        "length",
        "header",
    ],
    # lls.xml is a constants-only source: it defines buffer sizes and alignment
    # requirements, not packet structs, so pack/unpack/length/header cannot be
    # expected here. Check the constants it actually generates instead.
    "lls.h": [
        "VOLCANIC_LLS",
        "VOLCANIC_LLS_CDM_CONTEXT_RESUME_BUFFER_SIZE",
        "VOLCANIC_LLS_TA_STATE_BUFFER_SIZE",
        "VOLCANIC_LLS_USC_SHARED_REGS_BUFFER_SIZE",
        "VOLCANIC_LLS_TA_STATE_BUFFER_ALIGNMENT",
    ],
    "pbestate.h": [
        "VOLCANIC_PBESTATE",
        "VOLCANIC_PBESTATE_PBESTATE_REG_WORD0",  # specific struct from earlier review
        "pack",
        "unpack",
        "Bits:",              # bitfield description present
        "length",
    ],
    "pds.h": [
        "VOLCANIC_PDS",
        "pack",
        "unpack",
        "length",
        "header",
    ],
    # csbgen/volcanic/ppp.xml declares name="VOLCANIC" prefix="TA", so the
    # generated symbols are VOLCANIC_TA_*; "VOLCANIC_PPP" never appears.
    "ppp.h": [
        "VOLCANIC_TA_",
        "VOLCANIC_TA_TA_REG_WORD0",
        "VOLCANIC_TA_CLIP_MODE",
        "pack",
        "unpack",
        "length",
        "header",
    ],
    "texstate.h": [
        "VOLCANIC_TEXSTATE",
        "VOLCANIC_TEXSTATE_TEX_IMAGE_WORD0",  # specific struct from earlier review
        "pack",
        "unpack",
        "Bits:",              # bitfield description present
        "length",
    ],
    "vdm.h": [
        "VOLCANIC_VDMCTRL",
        "VOLCANIC_VDMCTRL_BLOCK_TYPE",  # enum with 7 values
        "PPP_STATE_UPDATE",             # enum value
        "STREAM_TERMINATE",             # enum value
        "pack",
        "unpack",
        "length",
    ],
}


def check_header_content(header_name, checks):
    """Check that a header file contains all required patterns."""
    path = os.path.join(VOLCANIC_HWDEF_DIR, header_name)
    if not os.path.exists(path):
        return False, f"File not found: {path}"

    with open(path, "r") as f:
        content = f.read()

    missing = []
    for check in checks:
        if check not in content:
            missing.append(check)

    if missing:
        return False, f"Missing: {', '.join(missing)}"
    return True, "All checks passed"


def main():
    print("=" * 60)
    print("LEVEL 2 CI TEST: Header Content Validation")
    print("Volcanic (BXM-8-256) Generated C Headers")
    print("=" * 60)
    print(f"Host: {os.uname().sysname} {os.uname().release}")
    print(f"Headers directory: {VOLCANIC_HWDEF_DIR}")
    print()

    all_pass = True
    total = len(HEADER_CHECKS)

    for header_name, checks in HEADER_CHECKS.items():
        success, message = check_header_content(header_name, checks)
        status = "PASS" if success else "FAIL"
        print(f"  {header_name:15s}: {status} - {message}")
        if not success:
            all_pass = False

    print()
    print("=" * 60)
    print(f"Results: {total} headers validated")
    print(f"  {sum(1 for r in [check_header_content(h, c) for h, c in HEADER_CHECKS.items()] if r[0])}/{total} headers: all checks passed")
    if all_pass:
        print("  ✅ ALL HEADER VALIDATION TESTS PASSED")
    else:
        print("  ⚠️  Some headers have missing content - see details above")
    print("=" * 60)

    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
