#!/usr/bin/env python3
"""
Find where an AArch64 ELF references a string, and show the surrounding code.

Used for the vendor-binary forensics in M19/M22: the PowerVR user-mode modules
are stripped, so a diagnostic string is often the only anchor to a piece of
logic. This locates the string, computes its virtual address, scans the
disassembly for the ADRP/ADD (or ADR) pair that materialises it, and prints the
instructions around each reference.

Requires binutils' aarch64-linux-gnu-objdump and readelf on PATH.

Example:

    python3 research/xref_string_aarch64.py \\
        binaries/libusc.so "In bindless mode the last gather4 source" --context 8
"""

import argparse
import re
import shutil
import struct
import subprocess
import sys

TOOL_PREFIXES = ("aarch64-linux-gnu-", "aarch64-none-linux-gnu-", "")


def find_tool(name):
    for prefix in TOOL_PREFIXES:
        path = shutil.which(prefix + name)
        if path:
            return path
    sys.exit(f"error: no {name} found (install binutils for aarch64)")


def sections(objdump_tool, path):
    """Returns [(name, addr, offset, size)] from readelf -S -W."""
    out = subprocess.run(
        [find_tool("readelf"), "-S", "-W", path],
        capture_output=True, text=True, check=True,
    ).stdout
    result = []
    for line in out.splitlines():
        m = re.match(
            r"\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)",
            line,
        )
        if m:
            name, addr, offset, size = m.group(1), *(int(g, 16) for g in m.groups()[1:])
            result.append((name, addr, offset, size))
    return result


def string_va(blob, text, secs):
    raw = text.encode()
    hits = []
    start = 0
    while True:
        idx = blob.find(raw, start)
        if idx < 0:
            break
        for name, addr, offset, size in secs:
            if offset <= idx < offset + size and size:
                hits.append((name, addr + (idx - offset), idx))
                break
        start = idx + 1
    return hits


def disassemble(path):
    out = subprocess.run(
        [find_tool("objdump"), "-d", "--no-show-raw-insn", path],
        capture_output=True, text=True, check=True,
    ).stdout
    insns = []
    for line in out.splitlines():
        m = re.match(r"^\s+([0-9a-f]+):\t(.*)$", line)
        if m:
            insns.append((int(m.group(1), 16), m.group(2).strip()))
    return insns


def find_xrefs(insns, va):
    """Returns instruction indices that materialise `va` via adrp/add or adr."""
    page, low = va & ~0xFFF, va & 0xFFF
    hits = []
    for i, (addr, text) in enumerate(insns):
        m = re.search(r"\badrp\s+x(\d+),\s*(?:0x)?([0-9a-f]+)\b", text)
        if m and int(m.group(2), 16) == page:
            reg = m.group(1)
            for j in range(i + 1, min(i + 4, len(insns))):
                m2 = re.search(
                    r"\badd\s+x%s,\s*x%s,\s*#?(?:0x)?([0-9a-f]+)\b"
                    % (reg, reg),
                    insns[j][1],
                )
                if m2 and int(m2.group(1), 16) == low:
                    hits.append(i)
                    break
            continue
        m = re.search(r"\badr\s+x\d+,\s*(?:0x)?([0-9a-f]+)\b", text)
        if m and int(m.group(1), 16) == va:
            hits.append(i)
    return hits


def nearest_symbol(insns, idx, path):
    """Best-effort name: the closest preceding <sym+off> objdump knows about."""
    for i in range(idx, -1, -1):
        if "<" in insns[i][1] or re.search(r"<\S+>", insns[i][1]):
            return insns[i][1]
    return "(unknown)"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf")
    ap.add_argument("string")
    ap.add_argument("--context", type=int, default=12,
                    help="instructions to show before each reference (default 12)")
    args = ap.parse_args()

    blob = open(args.elf, "rb").read()
    secs = sections(None, args.elf)
    hits = string_va(blob, args.string, secs)
    if not hits:
        print(f"not found: {args.string!r}")
        return 1

    for name, va, offset in hits:
        print(f"string at file offset 0x{offset:x}, section {name}, VA 0x{va:x}")

    insns = disassemble(args.elf)
    total = 0
    for _, va, _ in hits:
        refs = find_xrefs(insns, va)
        print(f"\n{len(refs)} xref(s) to VA 0x{va:x}")
        for idx in refs:
            total += 1
            print(f"\n  near 0x{insns[idx][0]:x} ({nearest_symbol(insns, idx, args.elf)})")
            for addr, text in insns[max(0, idx - args.context):idx + 3]:
                print(f"    {addr:x}: {text}")

    return 0 if total else 2


if __name__ == "__main__":
    sys.exit(main())
