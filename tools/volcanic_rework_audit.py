#!/usr/bin/env python3
"""Audit the Mesa PowerVR tree for Rogue- vs Volcanic-architecture drift.

This is a conservative, host-only rework starter. It scans the Imagination tree
for explicit Rogue markers and Volcanic markers so the project can separate
historical compatibility scaffolding from actual architecture truth.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TARGET = ROOT / "research" / "mesa-src" / "src" / "imagination"

ROGUE_PATTERNS = [
    r"PVR_DEVICE_ARCH_ROGUE",
    r"PER_ARCH_FUNCS\(rogue\)",
    r"PVR_BUILD_ARCH_ROGUE",
    r"\bROGUE_",
    r"rogue_get_render_size_max",
    r"rogue_hw_utils",
]

VOLCANIC_PATTERNS = [
    r"PVR_DEVICE_ARCH_VOLCANIC",
    r"PER_ARCH_FUNCS\(volcanic\)",
    r"PVR_BUILD_ARCH_VOLCANIC",
    r"\bVOLCANIC_",
    r"volcanic/",
]

file_exts = {".c", ".h", ".md", ".txt", ".py", ".build", ".mk"}


def scan_tree() -> tuple[dict[str, int], dict[str, list[str]]]:
    rogue_counts: dict[str, int] = {}
    rogue_files: dict[str, list[str]] = {}
    volatile_hit_files: set[str] = set()

    for path in sorted(TARGET.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in file_exts:
            continue

        try:
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue

        rogue_hits = []
        for pattern in ROGUE_PATTERNS:
            if re.search(pattern, text):
                rogue_hits.append(pattern)

        volcanic_hits = []
        for pattern in VOLCANIC_PATTERNS:
            if re.search(pattern, text):
                volcanic_hits.append(pattern)

        if rogue_hits:
            rogue_counts[str(path.relative_to(ROOT))] = len(rogue_hits)
            rogue_files[str(path.relative_to(ROOT))] = rogue_hits

        if volcanic_hits:
            volatile_hit_files.add(str(path.relative_to(ROOT)))

    return rogue_counts, rogue_files


def main() -> int:
    rogue_counts, rogue_files = scan_tree()
    rogue_total = sum(rogue_counts.values())

    print("====================================================")
    print("Volcanic Rework Audit: Rogue vs Volcanic drift scan")
    print("====================================================")
    print(f"Target root: {TARGET}")
    print(f"Files with Rogue markers: {len(rogue_counts)}")
    print(f"Total Rogue pattern hits: {rogue_total}")
    print()

    if not rogue_counts:
        print("No Rogue markers found in the Imagination subtree.")
        return 0

    print("Top Rogue marker files:")
    for rel, hits in sorted(rogue_counts.items(), key=lambda x: (-x[1], x[0]))[:20]:
        print(f"  - {rel}: {hits} hits")

    print()
    print("Key architecture-sensitive hotspots:")
    for rel in sorted(rogue_counts.keys()):
        if rel.endswith("bxm-8-256.h") or rel.endswith("pvr_physical_device.c") or rel.endswith("pvr_formats.c") or rel.endswith("README.md"):
            print(f"  - {rel}")

    print()
    print("Interpretation:")
    print("  - The repo still has explicit Rogue architecture markers in key driver files.")
    print("  - Rework should therefore start by decoupling the device profile, format table,")
    print("    and physical-device dispatch from legacy Rogue assumptions before M17+ work.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
