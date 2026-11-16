#!/usr/bin/env python3
"""
Decode AArch64 jump tables used by libusc.so's opcode dispatchers.

The dispatcher pattern in libusc.so is:

    ldrh  w11, [x9, x8, lsl #1]   ; 16-bit offset from a table
    adr   x10, <base>
    add   x10, x10, x11, lsl #2   ; base + 4 * offset
    br    x10

so the target for opcode N is base + 4 * table[N]. This decodes the table,
groups opcodes by target, and (with --resolve) follows each target block to the
function it tail-calls, which is how the opcode -> emitter mapping is recovered
without trusting instruction patterns.

Kept importable: test_m19_opcode_tables.py pins these findings with it.

Usage:
  jump_table.py <elf> <table-va> <base-addr> <count> [--resolve] [--start N]
"""

import argparse
import re
import struct
import subprocess
import sys

OBJDUMP = 'aarch64-linux-gnu-objdump'


def read_sections(elf):
    """Return [(name, addr, offset, size)] from readelf -SW."""
    out = subprocess.run(['readelf', '-SW', elf], capture_output=True, text=True).stdout
    secs = []
    for ln in out.splitlines():
        m = re.match(r'\s*\[\s*\d+\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)', ln)
        if m:
            secs.append((m.group(1), int(m.group(3), 16), int(m.group(4), 16), int(m.group(5), 16)))
    return secs


def section_of(sections, va):
    for sec in sections:
        name, addr, off, size = sec
        if addr and addr <= va < addr + size:
            return sec
    return None


def va_to_offset(sections, va):
    sec = section_of(sections, va)
    if sec is None:
        return None
    _, addr, off, _ = sec
    return off + (va - addr)


def read_u16(elf, sections, va):
    off = va_to_offset(sections, va)
    if off is None:
        return None
    with open(elf, 'rb') as f:
        f.seek(off)
        data = f.read(2)
    if len(data) != 2:
        return None
    return struct.unpack('<H', data)[0]


def read_u32(elf, sections, va):
    off = va_to_offset(sections, va)
    if off is None:
        return None
    with open(elf, 'rb') as f:
        f.seek(off)
        data = f.read(4)
    if len(data) != 4:
        return None
    return struct.unpack('<I', data)[0]


def read_cstr(elf, sections, va):
    off = va_to_offset(sections, va)
    if off is None:
        return None
    with open(elf, 'rb') as f:
        f.seek(off)
        data = f.read(256)
    end = data.find(b'\x00')
    if end < 0:
        return None
    return data[:end].decode('latin1')


def decode_table(elf, sections, table_va, base_va, count, start=0):
    """opcode -> target address for a 16-bit-offset jump table."""
    targets = {}
    for i in range(count):
        raw = read_u16(elf, sections, table_va + 2 * i)
        if raw is None:
            raise SystemExit('cannot read jump-table entry %d' % i)
        targets[i + start] = base_va + 4 * raw
    return targets


def block_tail(elf, addr, window=0x30):
    """The unconditional branch that terminates the case block at addr."""
    out = subprocess.run(
        [OBJDUMP, '-d', '--no-show-raw-insn',
         '--start-address=0x%x' % addr, '--stop-address=0x%x' % (addr + window), elf],
        capture_output=True, text=True).stdout
    for ln in out.splitlines():
        m = re.match(r'^\s+([0-9a-f]+):\tb\t([0-9a-f]+)', ln)
        if m:
            return int(m.group(2), 16)
    return None


def reloc_addends(elf, sections):
    """r_offset -> addend for R_AARCH64_RELATIVE, used to resolve name pointers."""
    sec = None
    for s in sections:
        if s[0] == '.rela.dyn':
            sec = s
            break
    if sec is None:
        return {}
    _, addr, off, size = sec
    out = {}
    with open(elf, 'rb') as f:
        f.seek(off)
        data = f.read(size)
    for i in range(0, len(data) - 23, 24):
        r_offset, _r_info, r_addend = struct.unpack('<QQq', data[i:i + 24])
        out[r_offset] = r_addend
    return out


def compact(ops):
    """Print opcode lists as ranges."""
    out = []
    start = prev = ops[0]
    for o in ops[1:]:
        if o == prev + 1:
            prev = o
            continue
        out.append('%#x' % start if start == prev else '%#x-%#x' % (start, prev))
        start = prev = o
    out.append('%#x' % start if start == prev else '%#x-%#x' % (start, prev))
    return ','.join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('elf')
    ap.add_argument('table', help='table virtual address (hex)')
    ap.add_argument('base', help='base address the offsets are relative to (hex)')
    ap.add_argument('count', type=lambda v: int(v, 0))
    ap.add_argument('--start', type=lambda v: int(v, 0), default=0,
                    help='opcode added to the index (for tables indexed by op - K)')
    ap.add_argument('--resolve', action='store_true')
    args = ap.parse_args()

    elf = args.elf
    sections = read_sections(elf)
    targets = decode_table(elf, sections, int(args.table, 0), int(args.base, 0),
                           args.count, args.start)

    groups = {}
    for op, target in targets.items():
        groups.setdefault(target, []).append(op)

    for target in sorted(groups):
        ops = groups[target]
        desc = ''
        if args.resolve:
            tail = block_tail(elf, target)
            if tail is not None:
                desc = ' -> b 0x%x' % tail
        print('target 0x%-8x opcodes %s%s' % (target, compact(sorted(ops)), desc))


if __name__ == '__main__':
    main()
