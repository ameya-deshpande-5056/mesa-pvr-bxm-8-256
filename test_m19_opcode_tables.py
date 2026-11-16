#!/usr/bin/env python3
"""
Pins the M19 findings about libusc.so's garden4 emit path.

Two facts are asserted, both recovered in the M19 analysis and both cheap to
regress if someone mistunes an address:

  1. Dispatcher A's jump table (VA 0xa088, base 0x1eb6cc, 0x185 entries) sends
     every opcode from 0x60 to 0x6e to one case block, and that block tail-calls
     the emitter at 0x1e8ea4.
  2. The per-opcode record table at VA 0x287138 (16-byte entries, third qword is
     a name pointer resolved through .rela.dyn) names 0x69 "LDLOD" and 0x6a
     "LDGATHER4".

The vendor blob is gitignored, so on a machine without it (including CI) this
check reports SKIP and exits 0. It never passes silently: the skip line says why.

Run: python3 test_m19_opcode_tables.py [path/to/libusc.so]
     PVR_LIBUSC=/path/to/libusc.so python3 test_m19_opcode_tables.py
"""

import os
import shutil
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'research'))

import jump_table  # noqa: E402

ELF = (sys.argv[1] if len(sys.argv) > 1
       else os.environ.get('PVR_LIBUSC', 'binaries/libusc.so'))

DISPATCHER_A_TABLE = 0xa088
DISPATCHER_A_BASE = 0x1eb6cc
DISPATCHER_A_COUNT = 0x185
EMITTER = 0x1e8ea4

OPCODE_RECORD_BASE = 0x287138
OPCODE_RECORD_STRIDE = 16

checks = 0
failures = []


def check(cond, msg):
    global checks
    checks += 1
    if not cond:
        failures.append(msg)
    print('  %s: %s' % ('PASS' if cond else 'FAIL', msg))


def main():
    if not os.path.exists(ELF):
        print('SKIP: vendor blob %s not present (gitignored); M19 opcode tables '
              'not verified on this host' % ELF)
        return 0
    if not shutil.which('readelf') or not shutil.which(jump_table.OBJDUMP):
        print('SKIP: readelf/objdump unavailable; M19 opcode tables not verified')
        return 0

    print('Checking M19 opcode tables in %s' % ELF)
    sections = jump_table.read_sections(ELF)
    targets = jump_table.decode_table(ELF, sections, DISPATCHER_A_TABLE,
                                      DISPATCHER_A_BASE, DISPATCHER_A_COUNT)

    # 1. The whole 0x60-0x6e family shares one case block.
    family = list(range(0x60, 0x6f))
    family_targets = {targets[op] for op in family}
    check(len(family_targets) == 1,
          'opcodes 0x60-0x6e share one dispatch target (%s)'
          % (', '.join('0x%x' % t for t in sorted(family_targets)) or 'none'))

    target = min(family_targets) if family_targets else 0
    tail = jump_table.block_tail(ELF, target) if target else None
    check(tail == EMITTER,
          'that case block tail-calls the emitter 0x%x (found 0x%x)'
          % (EMITTER, tail or 0))

    # Sanity: the emitter must be inside .text, not a stray address.
    sec = jump_table.section_of(sections, EMITTER)
    check(sec is not None and sec[0] == '.text',
          'emitter 0x%x lies in .text (section %s)' % (EMITTER, sec[0] if sec else 'none'))

    # 2. The opcode record table names the family members.
    addends = jump_table.reloc_addends(ELF, sections)

    def opcode_name(op):
        ptr_field = OPCODE_RECORD_BASE + OPCODE_RECORD_STRIDE * op + 8
        addend = addends.get(ptr_field)
        if addend is None:
            return None
        return jump_table.read_cstr(ELF, sections, addend)

    check(opcode_name(0x69) == 'LDLOD',
          'opcode 0x69 is named LDLOD (found %r)' % opcode_name(0x69))
    check(opcode_name(0x6a) == 'LDGATHER4',
          'opcode 0x6a is named LDGATHER4 (found %r)' % opcode_name(0x6a))

    # The distinction the correction depends on: 0x69 is not a gather4 opcode.
    check(opcode_name(0x69) != opcode_name(0x6a),
          'LDLOD and LDGATHER4 are distinct table entries')

    for op, want in ((0x6b, 'LDGATHER4_C'), (0x6c, 'LDGATHER4_PO'), (0x6d, 'LDGATHER4_C_PO')):
        got = opcode_name(op)
        check(got == want, 'opcode 0x%x is named %s (found %r)' % (op, want, got))

    # src count of LDGATHER4 must be the odd 7 that rules out "texture with
    # sampler" assumptions; dst count is 1.
    dst = jump_table.read_u32(ELF, sections, OPCODE_RECORD_BASE + OPCODE_RECORD_STRIDE * 0x6a)
    src = jump_table.read_u32(ELF, sections, OPCODE_RECORD_BASE + OPCODE_RECORD_STRIDE * 0x6a + 4)
    check((dst, src) == (1, 7),
          'LDGATHER4 declares 1 destination and 7 sources (found %s, %s)' % (dst, src))

    print('\n== %d passed, %d failed ==' % (checks - len(failures), len(failures)))
    if failures:
        for f in failures:
            print('  FAILED: %s' % f)
        return 1
    print('>>> M19 OPCODE TABLE CHECKS PASSED <<<')
    return 0


if __name__ == '__main__':
    sys.exit(main())
