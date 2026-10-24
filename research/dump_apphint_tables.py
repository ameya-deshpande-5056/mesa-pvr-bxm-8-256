import struct
import subprocess

with open("firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko", "rb") as f:
    elf_data = f.read()

rodata_off = 0x101970
rodata_str_off = 0xcb29c

p = subprocess.Popen(["readelf", "-r", "firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko"], stdout=subprocess.PIPE, text=True)
stdout, _ = p.communicate()

rodata_relocs = {}
in_rodata = False
for line in stdout.splitlines():
    if "Relocation section '.rela.rodata'" in line:
        in_rodata = True
        continue
    if in_rodata and "Relocation section" in line:
        break
    if in_rodata and line.strip():
        parts = line.split()
        if len(parts) >= 5 and parts[2] == "R_AARCH64_ABS64":
            off = int(parts[0], 16)
            sym_name = parts[4]
            addend = 0
            if len(parts) >= 7 and parts[5] == "+":
                addend = int(parts[6], 16)
            rodata_relocs[off] = (sym_name, addend)

tables = [
    ("Global Table 1", 0xd88, 12),
    ("Global Table 2", 0xf08, 6),
    ("Global Table 3", 0x11f8, 83),
]

types = {
    0: "UINT32",
    1: "UINT64",
    2: "STRING",
    3: "BOOL",
    4: "INPUT_STREAM",
    5: "OUTPUT_STREAM",
    6: "CUSTOM"
}

all_apphints = {}

for tname, start_off, count in tables:
    for i in range(count):
        entry_off = start_off + i * 32
        data_slice = elf_data[rodata_off + entry_off : rodata_off + entry_off + 32]
        aid, atype = struct.unpack_from("<II", data_slice, 0)
        flags = struct.unpack_from("<I", data_slice, 24)[0]
        
        name = "UNKNOWN"
        r = rodata_relocs.get(entry_off + 8)
        if r:
            sym_name, addend = r
            if sym_name == ".rodata.str1.1":
                s_start = rodata_str_off + addend
                s_end = elf_data.find(b"\0", s_start)
                name = elf_data[s_start:s_end].decode("ascii", errors="replace")
            elif sym_name == ".rodata":
                s_start = rodata_off + addend
                s_end = elf_data.find(b"\0", s_start)
                name = elf_data[s_start:s_end].decode("ascii", errors="replace")
            else:
                name = f"{sym_name}+{hex(addend)}"
                
        all_apphints[aid] = (name, atype, flags, entry_off)

print(f"{'ID':<4} {'Hex':<5} {'AppHint Name':<35} {'Type':<15} {'Table Entry'}")
print("-" * 75)
for aid in sorted(all_apphints.keys()):
    name, atype, flags, entry_off = all_apphints[aid]
    print(f"{aid:<4} 0x{aid:02x}  {name:<35} {types.get(atype, str(atype)):<15} .rodata+0x{entry_off:04x}")

