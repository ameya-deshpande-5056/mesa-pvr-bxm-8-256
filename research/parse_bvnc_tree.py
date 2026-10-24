import subprocess

p = subprocess.Popen(["aarch64-linux-gnu-objdump", "-d", "--start-address=0x8f2a0", "--stop-address=0x8fd60", "firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko"], stdout=subprocess.PIPE, text=True)
stdout, _ = p.communicate()

bvncs = []
lines = stdout.splitlines()
regs = {}

for line in lines:
    parts = line.split(":\t")
    if len(parts) < 2:
        continue
    addr = parts[0].strip()
    rest = parts[1].split()
    opcode = rest[1]
    args = "".join(rest[2:])
    
    if opcode == "mov":
        r_parts = args.split(",")
        r = r_parts[0]
        v_str = r_parts[1].replace("#", "").split("//")[0].strip()
        try:
            regs[r] = int(v_str, 0)
        except ValueError:
            if v_str in regs:
                regs[r] = regs[v_str]
    elif opcode == "movk":
        r_parts = args.split(",")
        r = r_parts[0]
        v_str = r_parts[1].replace("#", "").strip()
        v = int(v_str, 0)
        shift = 0
        if len(r_parts) > 2 and "lsl" in r_parts[2]:
            s_str = r_parts[2].replace("lsl#", "").replace("lsl", "").replace("#", "").split("//")[0].strip()
            shift = int(s_str, 0)
        mask = ~(0xffff << shift) & 0xffffffffffffffff
        regs[r] = (regs.get(r, 0) & mask) | (v << shift)
    elif opcode == "cmp":
        if "x8," in args:
            comp_reg = args.split(",")[1].split("//")[0].strip()
            val = regs.get(comp_reg, None)
            if val is not None:
                b = (val >> 48) & 0xffff
                n = (val >> 16) & 0xffff
                c = val & 0xffff
                bvncs.append((addr, comp_reg, val, b, n, c))

print(f"Total BVNC comparisons found: {len(bvncs)}")
for addr, r, val, b, n, c in bvncs:
    print(f"[{addr}] cmp x8, {r}: 0x{val:016x} -> B={b:2d}, N={n:4d}, C={c:4d}")
