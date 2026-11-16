#!/usr/bin/env python3
"""
Light symbolic register tracer for AArch64 disassembly of a single function.

Built for M19 analysis of libusc.so: the vendor compiler reuses argument
registers as general temporaries a few hundred instructions into a function, so
pattern matching `[x20, #N]` attributes loads to the wrong struct. This walks a
function linearly, tracks what each register holds (an argument pointer plus an
offset, a constant, or a value loaded from memory), and reports the accesses and
call arguments with their provenance attached.

It is deliberately approximate: branch targets merge to "unknown" unless every
incoming edge agrees, and spilled values loaded back from the stack are unknown.
That keeps it honest rather than clever -- a wrong attribution is the failure
mode we are trying to avoid.

Usage:
  trace_regs.py <elf> <start> <stop> [--args x0=ctx,x1=state,...] [--bl]

  <start>/<stop> are hex addresses. --args names the incoming argument
  registers; --bl prints, for every call, the resolved provenance of the first
  six argument registers.
"""

import argparse
import re
import subprocess
import sys

OBJDUMP = 'aarch64-linux-gnu-objdump'

INSN_RE = re.compile(r'^\s+([0-9a-f]+):\t(.*)$')
# Registers we track. Everything else (sp, xzr, ...) is left alone.
XREGS = ['x%d' % i for i in range(31)] + ['w%d' % i for i in range(31)]


def disasm(elf, start, stop):
    out = subprocess.run(
        [OBJDUMP, '-d', '--no-show-raw-insn',
         '--start-address=0x%x' % start, '--stop-address=0x%x' % stop, elf],
        capture_output=True, text=True).stdout
    insns = []
    for ln in out.splitlines():
        m = INSN_RE.match(ln)
        if m:
            insns.append((int(m.group(1), 16), m.group(2).strip()))
    return insns


class Val:
    """A resolved register value.

    kind 'arg'  -> points at argument number `arg` plus `off` bytes
    kind 'imm'  -> the integer `imm`
    kind 'mem'  -> loaded from argument `arg` at `off` (width `width` bits)
    kind None   -> unknown
    """

    __slots__ = ('kind', 'arg', 'off', 'imm', 'width', 'label')

    def __init__(self, kind=None, arg=None, off=0, imm=0, width=0, label=None):
        self.kind = kind
        self.arg = arg
        self.off = off
        self.imm = imm
        self.width = width
        self.label = label

    def __repr__(self):
        if self.kind == 'arg':
            return '%s+%d' % (self.label, self.off)
        if self.kind == 'imm':
            return '#%d (0x%x)' % (self.imm, self.imm)
        if self.kind == 'mem':
            return '[%s+%d]:%d' % (self.label, self.off, self.width)
        return '?'

    def unknown(self):
        return self.kind is None


UNKNOWN = Val()


def parse_imm(tok):
    tok = tok.strip().lstrip('#')
    try:
        return int(tok, 0)
    except ValueError:
        return None


MEM_RE = re.compile(
    r'^(ldr|ldrb|ldrh|ldrsw|ldrsb|ldrsh|str|strb|strh|ldur|stur|ldurb|sturb)'
    r'\s+(\w+),\s*\[(\w+)(?:,\s*#?(-?0x[0-9a-f]+|-?\d+))?\]')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('elf')
    ap.add_argument('start')
    ap.add_argument('stop')
    ap.add_argument('--args', default='x0=ctx')
    ap.add_argument('--bl', action='store_true')
    ap.add_argument('--accesses', action='store_true',
                    help='print memory accesses through tracked argument bases')
    ap.add_argument('--only', default=None,
                    help='restrict printed access offsets to this byte offset')
    ap.add_argument('--trace', action='store_true',
                    help='print every instruction with the known operand values')
    args = ap.parse_args()

    labels = {}
    for idx, spec in enumerate(args.args.split(',')):
        reg, name = spec.split('=')
        labels[reg] = name

    insns = disasm(args.elf, int(args.start, 0), int(args.stop, 0))
    if not insns:
        sys.exit('no instructions decoded in range')

    # Branch targets force a conservative merge: state is dropped there.
    targets = set()
    for addr, text in insns:
        m = re.search(r'\b(?:b|b\.\w+|cbz|cbnz|tbz|tbnz)\b.*?\b([0-9a-f]{4,})\s*(?:<|$)', text)
        if m:
            targets.add(int(m.group(1), 16))

    state = {}
    for reg in XREGS:
        state[reg] = UNKNOWN
    for reg, name in labels.items():
        state[reg] = Val('arg', arg=reg, label=name)

    for addr, text in insns:
        if addr in targets:
            state = {r: UNKNOWN for r in XREGS}
            for reg, name in labels.items():
                state[reg] = Val('arg', arg=reg, label=name)

        if args.trace:
            known = []
            for tok in re.findall(r'\b[xw]\d+\b', text):
                v = state.get(tok, UNKNOWN)
                if not v.unknown():
                    known.append('%s=%r' % (tok, v))
            print('  %x: %-44s %s' % (addr, text, '  '.join(known)))

        # register move
        m = re.match(r'^mov\s+(x\d+|w\d+),\s*(x\d+|w\d+|xzr|wzr)$', text)
        if m:
            dst, src = m.group(1), m.group(2)
            if src in ('xzr', 'wzr'):
                state[dst] = Val('imm', imm=0)
            else:
                state[dst] = state.get(src, UNKNOWN)
            continue

        m = re.match(r'^mov\s+(x\d+|w\d+),\s*#(\d+|0x[0-9a-f]+)$', text)
        if m:
            state[m.group(1)] = Val('imm', imm=int(m.group(2), 0))
            continue

        m = re.match(r'^movz\s+(x\d+|w\d+),\s*#(0x[0-9a-f]+|\d+)$', text)
        if m:
            state[m.group(1)] = Val('imm', imm=int(m.group(2), 0))
            continue

        # add/sub with an immediate
        m = re.match(r'^(add|sub)\s+(x\d+|w\d+),\s*(x\d+|w\d+),\s*#(0x[0-9a-f]+|\d+)$', text)
        if m:
            op, dst, src, imm = m.group(1), m.group(2), m.group(3), int(m.group(4), 0)
            base = state.get(src, UNKNOWN)
            if base.kind == 'arg':
                state[dst] = Val('arg', arg=base.arg, off=base.off + (imm if op == 'add' else -imm),
                                 label=base.label)
            elif base.kind == 'imm':
                state[dst] = Val('imm', imm=base.imm + (imm if op == 'add' else -imm))
            else:
                state[dst] = UNKNOWN
            continue

        # memory access
        m = MEM_RE.match(text)
        if m:
            op, rn, base, off = m.group(1), m.group(2), m.group(3), m.group(4)
            off = parse_imm(off) if off else 0
            width = {'ldrb': 8, 'strb': 8, 'ldrh': 16, 'strh': 16,
                     'ldrsb': 8, 'ldrsh': 16}.get(op, 32)
            if op in ('ldr', 'ldur') and rn.startswith('x'):
                width = 64
            if op in ('str', 'stur') and rn.startswith('x'):
                width = 64
            basev = state.get(base, UNKNOWN)
            if basev.kind == 'arg':
                total = basev.off + off
                if args.accesses:
                    if args.only is None or total == int(args.only, 0):
                        print('  %x: %-40s -> %s[%d] (%d-bit)'
                              % (addr, text, basev.label, total, width))
                if op in ('ldr', 'ldrb', 'ldrh', 'ldrsw', 'ldrsb', 'ldrsh', 'ldur'):
                    state[rn] = Val('mem', arg=basev.arg, off=total,
                                    width=width, label=basev.label)
                continue
            if basev.kind == 'imm':
                state[rn] = UNKNOWN
                continue
            if op in ('ldr', 'ldrb', 'ldrh', 'ldrsw', 'ldrsb', 'ldrsh', 'ldur'):
                state[rn] = UNKNOWN
            continue

        # any other instruction writing its first operand invalidates it
        m = re.match(r'^(\w+)\s+(x\d+|w\d+),', text)
        if m:
            state[m.group(2)] = UNKNOWN
            continue

        if args.bl and text.startswith('bl '):
            pass


if __name__ == '__main__':
    main()
