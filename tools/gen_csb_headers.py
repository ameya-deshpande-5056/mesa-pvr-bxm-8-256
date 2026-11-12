#!/usr/bin/env python3
"""
Regenerate the CSB packet headers (ppp.h, texstate.h, ...) into the path the
offline gates read.

The gates inspect the *generated* packet headers, because that is what the
driver compiles against: bit positions and enum values only exist there. A
fresh checkout has no build tree, so this runs `csbgen/gen_pack_header.py`
exactly the way `src/imagination/csbgen/<arch>/meson.build` does (one XML in,
captured stdout to <name>.h) and writes the result to

    research/mesa-src/build-android/src/imagination/csbgen/<arch>/<name>.h

It touches nothing else, so it is safe to run before a real build; a later
meson build simply overwrites the same files with identical content.

Usage: tools/gen_csb_headers.py
"""

from __future__ import annotations

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CSBGEN = ROOT / "research" / "mesa-src" / "src" / "imagination" / "csbgen"
OUT_ROOT = (
    ROOT / "research" / "mesa-src" / "build-android" / "src" / "imagination" / "csbgen"
)
GENERATOR = CSBGEN / "gen_pack_header.py"


def main() -> int:
    if not GENERATOR.is_file():
        print(
            f"error: {GENERATOR} is missing - initialise the mesa-src submodule "
            f"(git submodule update --init --recursive)",
            file=sys.stderr,
        )
        return 1

    generated = 0
    for arch in ("rogue", "volcanic"):
        src_dir = CSBGEN / arch
        if not src_dir.is_dir():
            continue

        out_dir = OUT_ROOT / arch
        out_dir.mkdir(parents=True, exist_ok=True)

        for xml in sorted(src_dir.glob("*.xml")):
            result = subprocess.run(
                [sys.executable, str(GENERATOR), str(xml)],
                capture_output=True,
                text=True,
            )
            if result.returncode != 0:
                print(
                    f"error: generating {xml.name} failed:\n{result.stderr}",
                    file=sys.stderr,
                )
                return 1
            if not result.stdout:
                print(f"error: {xml.name} produced no output", file=sys.stderr)
                return 1

            (out_dir / f"{xml.stem}.h").write_text(result.stdout)
            generated += 1

    print(f"generated {generated} CSB packet headers under "
          f"{OUT_ROOT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
