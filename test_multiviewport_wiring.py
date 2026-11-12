#!/usr/bin/env python3
"""
Offline gate for M21 multi-viewport support.

Checks the four places the driver must agree on viewport capacity before any
multi-viewport draw can ever happen:

1. The device feature table (bxm-8-256.h) advertises the hardware capability
   via has_multiviewport, and the Vulkan feature string advertises multiViewport
   only when that bit is set.
2. The compile-time constant PVR_MAX_VIEWPORTS is raised from 1 to a value that
   matches the feature capability, so the cmd-buffer viewports[] array is wide
   enough.
3. The Vulkan state limits report maxViewports >= 16, so applications know the
   cap (it must be at least the feature capability).
4. The multi-viewport draw path is wired (pvr_setup_viewport loops over
   viewport_count, pvr_emit_ppp_state emits the header view_port_count and all
   viewport[] entries, and pvr_emit_ppp_state never emits only [0]).
5. The device-info vendor dump agrees the feature is present (vkjson.json
   reports maxViewports == 16.0 and multiViewport == 1.0).

None of this requires a device. It is all static inspection of in-tree sources
and the vendor device-info packet.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
MESA_SRC = os.path.join(REPO_ROOT, "research", "mesa-src")

if not os.path.isdir(MESA_SRC):
    sys.exit(
        f"error: {MESA_SRC} is missing - initialise the mesa-src submodule "
        f"(git submodule update --init --recursive)"
    )

HWDEF_BXM = os.path.join(
    MESA_SRC, "src/imagination/common/device_info/bxm-8-256.h"
)
HWDEF_FEATURES = os.path.join(
    MESA_SRC, "src/imagination/common/pvr_device_info.h"
)
LIMITS_C = os.path.join(
    MESA_SRC,
    "src/imagination/vulkan/pvr_physical_device.c",
)
CMD_BUFFER_C = os.path.join(
    MESA_SRC,
    "src/imagination/vulkan/pvr_arch_cmd_buffer.c",
)
VULKAN_LIMITS_H = os.path.join(
    MESA_SRC, "src/vulkan/runtime/vk_limits.h"
)
# The vendor device-info packet is checked in at the repository root,
# outside the mesa-src submodule.
VKJSON = os.path.join(REPO_ROOT, "device-info", "vkjson.json")
VOLCANIC_XML = os.path.join(
    MESA_SRC, "src/imagination/csbgen/volcanic/ppp.xml"
)

_SRCS = [
    HWDEF_BXM,
    HWDEF_FEATURES,
    LIMITS_C,
    CMD_BUFFER_C,
    VULKAN_LIMITS_H,
    VOLCANIC_XML,
    VKJSON,
]

_TESTS: list[tuple[str, callable]] = []


def _read(path: str) -> str:
    try:
        return Path(path).read_text(encoding="utf-8")
    except FileNotFoundError:
        return ""


def _resolve_int_symbol(token: str, text: str) -> int | None:
    """Resolve a literal, or a macro defined in the inspected sources.

    The limits may legitimately name the cap (`PVR_MAX_VIEWPORTS`) instead of
    repeating the number, so the check follows the symbol to its #define rather
    than requiring a magic constant at the use site.
    """
    if token.isdigit():
        return int(token)
    m = re.search(r"#define\s+" + re.escape(token) + r"\s+(\d+)U?\b", text)
    if not m:
        return None
    return int(m.group(1))


def _run(cmd: str) -> tuple[bool, str]:
    try:
        r = subprocess.run(
            cmd, shell=True, capture_output=True, text=True, timeout=120
        )
        return r.returncode == 0, (r.stdout + r.stderr).strip()
    except Exception as exc:
        return False, str(exc)


# ---------------------------------------------------------------------------
# 1/5  Device feature table must advertise has_multiviewport, and the Vulkan
#      feature string must be gated on it.
# ---------------------------------------------------------------------------

def _feature_struct_has(viewpath: str, field: str) -> bool:
    text = _read(viewpath)
    m = re.search(
        rf"\.{field}\s*=\s*(true|false)\b", text, re.IGNORECASE
    )
    if not m:
        return False
    return m.group(1).lower() == "true"


def check_device_feature_table_has_multiviewport() -> bool:
    """bxm-8-256.h must define has_multiviewport = true in the feature table."""
    return _feature_struct_has(HWDEF_BXM, "has_multiviewport")


def check_vulkan_feature_multi_viewport_gated_on_feature_bit() -> bool:
    """pvr_physical_device.c must advertise multiViewport only when the
    device feature bit is set (PVR_HAS_FEATURE(dev_info, multiviewport))."""
    text = _read(LIMITS_C)
    has_guard = re.search(
        r"\.multiViewport\s*=\s*PVR_HAS_FEATURE\(dev_info,\s*multiviewport\)",
        text,
    )
    return bool(has_guard)


# ---------------------------------------------------------------------------
# 2/5  PVR_MAX_VIEWPORTS compile-time constant must be raised.
# ---------------------------------------------------------------------------

def check_pvr_max_viewports_value() -> bool:
    """PVR_MAX_VIEWPORTS must be >= 16 so the feature can be used."""
    text_pieces = [
        _read(LIMITS_C),
        _read(os.path.join(MESA_SRC, "src/imagination/common/pvr_limits.h")),
    ]
    text_full = "\n".join(p for p in text_pieces if p)
    m = re.search(r"#define\s+PVR_MAX_VIEWPORTS\s+(\d+)U?\b", text_full)
    if not m:
        return False
    value = int(m.group(1))
    return value >= 16


# ---------------------------------------------------------------------------
# 3/5  Vulkan state limits must report maxViewports >= 16.
# ---------------------------------------------------------------------------

def check_vulkan_max_viewports_minimum() -> bool:
    """maxViewports in the Vulkan physical-device limits must be at least 16,
    matching the feature capability."""
    text_pieces = [
        _read(LIMITS_C),
        _read(os.path.join(MESA_SRC, "src/imagination/common/pvr_limits.h")),
    ]
    text_full = "\n".join(p for p in text_pieces if p)
    m = re.search(r"\.maxViewports\s*=\s*(\w+)U?\b", text_full, re.MULTILINE)
    if not m:
        return False
    reported = _resolve_int_symbol(m.group(1), text_full)
    if reported is None:
        return False
    return reported >= 16


# ---------------------------------------------------------------------------
# 4/5  Multi-viewport draw path must be wired.
# ---------------------------------------------------------------------------

def _array_size_used_in_emit_loop(viewports_usage_text: str) -> bool:
    """The emit path should iterate over ppp_state->viewport_count entries,
    not only [0], and should not assume it is always 1."""
    has_dynamic_header_count = re.search(
        r"header->view_port_count\s*=\s*"
        r"\(\s*ppp_state->viewport_count\s*(==|!=)\s*0\s*\)"
        r"\s*\?\s*(?:0U\s*:\s*)?\(?\s*ppp_state->viewport_count\s*(?:-)?\s*1\s*\)?\s*;?",
        viewports_usage_text,
    )
    has_full_loop_emit = re.search(
        r"\bfor\s*\(\s*uint32_t\s+\w+\s*=\s*0\s*;\s*\w+\s*<\s*"
        r"(?:ppp_state->viewport_count|viewports)\b",
        viewports_usage_text,
    )
    return bool(has_dynamic_header_count) and bool(has_full_loop_emit)


def check_multi_viewport_emit_path() -> bool:
    """pvr_emit_ppp_state must emit the header view_port_count from the
    runtime viewport_count, and all viewport[] entries, not just [0]."""
    text = _read(CMD_BUFFER_C)
    return _array_size_used_in_emit_loop(text)


def check_viewport_setup_handles_multiple() -> bool:
    """pvr_setup_viewport must iterate over viewport_count entries (the
    per-viewport scaling arithmetic), not assume a single viewport."""
    text = _read(CMD_BUFFER_C)
    in_func = re.search(
        r"static\s+void\s+pvr_setup_viewport\s*\([^)]*\)\s*\{(.*?)\n\}\s*$",
        text,
        re.DOTALL,
    )
    if not in_func:
        return False
    body = in_func.group(1)
    has_loop = re.search(
        r"\bfor\s*\(\s*uint32_t\s+\w+\s*=\s*0\s*;\s*\w+\s*<\s*"
        r"ppp_state->viewport_count\b",
        body,
    )
    return bool(has_loop)


def check_region_clip_covers_all_viewports() -> bool:
    """The ISP scissor and TA region clip are single rectangles, so with more
    than one viewport they must be derived from every viewport/scissor pair.
    A leftover `viewport_count == 1` assertion here would either abort in a
    debug build or silently cull geometry in a release build."""
    text = _read(CMD_BUFFER_C)
    if re.search(r"assert\s*\([^;]*viewport_count\s*==\s*1\s*\)", text):
        return False
    if re.search(r"assert\s*\([^;]*scissor_count\s*==\s*1\s*\)", text):
        return False
    return bool(
        re.search(
            r"for\s*\(\s*uint32_t\s+\w+\s*=\s*0\s*;\s*\w+\s*<\s*"
            r"dynamic_state->vp\.viewport_count\b",
            text,
        )
    )


# ---------------------------------------------------------------------------
# 5/5  Vendor device-info agrees the capability is present.
# ---------------------------------------------------------------------------

def check_vendor_device_info_max_viewports_16() -> bool:
    """vkjson.json must report maxViewports == 16.0 (the B-Series capability
    observed from the device)."""
    text = _read(VKJSON)
    m = re.search(r'"maxViewports"\s*:\s*([\d.]+)\b', text)
    if not m:
        return False
    try:
        val = float(m.group(1))
    except ValueError:
        return False
    return val == 16.0


def check_vendor_device_info_multi_viewport_1() -> bool:
    """vkjson.json must report multiViewport == 1.0."""
    text = _read(VKJSON)
    m = re.search(r'"multiViewport"\s*:\s*([\d.]+)\b', text)
    if not m:
        return False
    try:
        val = float(m.group(1))
    except ValueError:
        return False
    return val == 1.0


# ===========================================================================
# Suite
# ===========================================================================

_TESTS = [
    ("device feature table has has_multiviewport = true", check_device_feature_table_has_multiviewport),
    ("Vulkan multiViewport gated on the feature bit", check_vulkan_feature_multi_viewport_gated_on_feature_bit),
    ("PVR_MAX_VIEWPORTS raised to >= 16", check_pvr_max_viewports_value),
    ("Vulkan maxViewports >= 16", check_vulkan_max_viewports_minimum),
    ("multi-viewport emit path wired", check_multi_viewport_emit_path),
    ("viewport setup loops over viewport_count", check_viewport_setup_handles_multiple),
    ("region clip/scissor union covers every viewport", check_region_clip_covers_all_viewports),
    ("vendor device-info maxViewports == 16.0", check_vendor_device_info_max_viewports_16),
    ("vendor device-info multiViewport == 1.0", check_vendor_device_info_multi_viewport_1),
]


def main() -> int:
    print("=" * 64)
    print("M21 MULTI-VIEWPORT GATE")
    print("=" * 64)
    print(f"Sources inspected: {len(_SRCS)}")
    for s in _SRCS:
        tag = "OK" if os.path.exists(s) else "MISSING"
        print(f"  [{tag}] {s}")
    print()

    passed = 0
    results: list[tuple[str, bool, str]] = []
    for desc, fn in _TESTS:
        ok = fn()
        results.append((desc, ok, "PASS" if ok else "FAIL"))
        if ok:
            passed += 1

    print("-" * 64)
    for desc, ok, label in results:
        print(f"  [{label}] {desc}")
        if not ok:
            print(f"         ^^^ FAILED")
    print("-" * 64)

    print()
    print(f"Checkpoint: {passed}/{len(_TESTS)} checks passed")
    if passed == len(_TESTS):
        print(f"  ✅ All M21 multi-viewport wiring checks pass")
        return 0
    print(f"  ⚠️  {len(_TESTS) - passed} check(s) failed")
    return 1


if __name__ == "__main__":
    sys.exit(main())
