import re

target = 0xfcb6
lines = open("libsrv_um.disasm").readlines()

for i, line in enumerate(lines):
    m = re.search(r'^\s*([0-9a-f]+):.*\badrp\s+x(\d+),\s*(0x[0-9a-f]+)', line)
    if not m:
        continue

    page = int(m.group(3), 16)
    reg = int(m.group(2))

    for other in lines[i+1:i+4]:
        a = re.search(
            rf'^\s*([0-9a-f]+):.*\badd\s+x{reg},\s*x{reg},\s*#(0x[0-9a-f]+)',
            other
        )
        if a and (page + int(a.group(2), 16)) == target:
            print(line.rstrip())
            print(other.rstrip())
            print()
