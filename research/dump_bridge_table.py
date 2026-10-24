import subprocess
import struct
import re

# Parse relocations from pvrsrvkm.ko
res_r = subprocess.check_output(["aarch64-linux-gnu-readelf", "-W", "-r", "firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko"]).decode()
relocs = {}
for line in res_r.splitlines():
    parts = line.split()
    if len(parts) >= 5:
        try:
            off = int(parts[0], 16)
            relocs[off] = " ".join(parts[4:])
        except ValueError:
            pass

# Read elf data for strings
with open("firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko", "rb") as f:
    elf_data = f.read()

# .rodata.str1.1 offset is 0xcb29c
str_base = 0xcb29c

def get_rodata_str(off):
    pos = str_base + off
    end = elf_data.find(b"\0", pos)
    if end == -1: return ""
    return elf_data[pos:end].decode("latin1", errors="ignore")

# Disassemble .text
res_d = subprocess.check_output(["aarch64-linux-gnu-objdump", "-d", "firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko"]).decode()
lines = res_d.splitlines()

bridge_entries = []

for i, line in enumerate(lines):
    if "<_SetDispatchTableEntry>" in line and "bl\t" in line:
        # Search backwards up to 35 instructions to extract arguments
        # w0: bridge_id
        # w1: func_id
        # x2: name string (.rodata.str1.1 + offset)
        # x3: handler function
        # w7: in_data_size
        # [sp]: out_data_size (via str w8, [sp] or similar)
        
        bridge_id = None
        func_id = None
        name = "UNKNOWN"
        handler = "UNKNOWN"
        in_size = None
        out_size = None
        
        # Track register values going backwards
        regs = {}
        sp_val = None
        
        start_k = max(0, i - 25)
        for k in range(start_k, i):
            l = lines[k].strip()
            addr_match = re.match(r"^([0-9a-f]+):", l)
            if not addr_match:
                continue
            addr = int(addr_match.group(1), 16)
            
            # Check relocations at this address
            if addr in relocs:
                r_target = relocs[addr]
                if ".rodata.str1.1 +" in r_target:
                    off_m = re.search(r"\+ ([0-9a-f]+)", r_target)
                    if off_m:
                        s_off = int(off_m.group(1), 16)
                        # Check destination register
                        if "adrp\tx2" in l or "add\tx2" in l:
                            name = get_rodata_str(s_off)
                elif ".text +" in r_target or any(sym in r_target for sym in ["Bridge", "PVRSRV", "RGX", "Sync"]):
                    if "adrp\tx3" in l or "add\tx3" in l:
                        handler = r_target
            
            # Match mov/str instructions
            m_mov = re.search(r"mov\t([wx][0-9]+),\s*#([0-9a-fx]+)", l)
            if m_mov:
                reg = m_mov.group(1)
                val = int(m_mov.group(2), 0)
                regs[reg] = val
                
            m_str_sp = re.search(r"str\t([wx][0-9]+),\s*\[sp\]", l)
            if m_str_sp:
                src_reg = m_str_sp.group(1)
                if src_reg in regs:
                    out_size = regs[src_reg]
                elif src_reg.replace("x", "w") in regs:
                    out_size = regs[src_reg.replace("x", "w")]

        if "w0" in regs: bridge_id = regs["w0"]
        if "w1" in regs: func_id = regs["w1"]
        if "w7" in regs: in_size = regs["w7"]
        
        bridge_entries.append({
            "bridge_id": bridge_id,
            "func_id": func_id,
            "name": name,
            "handler": handler,
            "in_size": in_size,
            "out_size": out_size,
            "call_addr": hex(addr)
        })

print(f"Parsed {len(bridge_entries)} bridge entries.")
# Print sorted by bridge_id, func_id
for e in sorted(bridge_entries, key=lambda x: (x["bridge_id"] or 999, x["func_id"] or 999)):
    print(f"Bridge {e['bridge_id']:3} | Func {str(e['func_id']):3} | In {str(e['in_size']):4} | Out {str(e['out_size']):4} | {e['name']:<45} | {e['handler']}")

