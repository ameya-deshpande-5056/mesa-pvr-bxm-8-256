import struct

with open("firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko", "rb") as f:
    elf_data = f.read()

# Parse ELF header
e_shoff = struct.unpack_from("<Q", elf_data, 40)[0]
e_shentsize = struct.unpack_from("<H", elf_data, 58)[0]
e_shnum = struct.unpack_from("<H", elf_data, 60)[0]
e_shstrndx = struct.unpack_from("<H", elf_data, 62)[0]

# Section headers
shstrtab_hdr = elf_data[e_shoff + e_shstrndx * e_shentsize : e_shoff + (e_shstrndx + 1) * e_shentsize]
shstrtab_off = struct.unpack_from("<Q", shstrtab_hdr, 24)[0]

sections = []
for i in range(e_shnum):
    sh = elf_data[e_shoff + i * e_shentsize : e_shoff + (i + 1) * e_shentsize]
    sh_name_idx = struct.unpack_from("<I", sh, 0)[0]
    sh_type = struct.unpack_from("<I", sh, 4)[0]
    sh_offset = struct.unpack_from("<Q", sh, 24)[0]
    sh_size = struct.unpack_from("<Q", sh, 32)[0]
    sh_link = struct.unpack_from("<I", sh, 40)[0]
    sh_info = struct.unpack_from("<I", sh, 44)[0]
    
    # get name
    name_end = elf_data.find(b"\0", shstrtab_off + sh_name_idx)
    name = elf_data[shstrtab_off + sh_name_idx : name_end].decode("ascii", errors="replace")
    sections.append({
        "index": i, "name": name, "type": sh_type, "offset": sh_offset,
        "size": sh_size, "link": sh_link, "info": sh_info
    })

sec_by_name = {s["name"]: s for s in sections}

# Read symbol table
symtab_sec = sec_by_name[".symtab"]
strtab_sec = sections[symtab_sec["link"]]

symbols = []
for off in range(symtab_sec["offset"], symtab_sec["offset"] + symtab_sec["size"], 24):
    st_name = struct.unpack_from("<I", elf_data, off)[0]
    st_info = struct.unpack_from("<B", elf_data, off + 4)[0]
    st_other = struct.unpack_from("<B", elf_data, off + 5)[0]
    st_shndx = struct.unpack_from("<H", elf_data, off + 6)[0]
    st_value = struct.unpack_from("<Q", elf_data, off + 8)[0]
    st_size = struct.unpack_from("<Q", elf_data, off + 16)[0]
    
    name_end = elf_data.find(b"\0", strtab_sec["offset"] + st_name)
    sname = elf_data[strtab_sec["offset"] + st_name : name_end].decode("ascii", errors="replace")
    symbols.append({
        "name": sname, "info": st_info, "shndx": st_shndx, "value": st_value, "size": st_size
    })

# Read rela.__param
rela_param_sec = sec_by_name[".rela__param"]
param_sec = sec_by_name["__param"]
rodata_sec = sec_by_name[".rodata"]
data_sec = sec_by_name[".data"]

# Build relocation dict for __param: offset -> (symbol_name, addend)
param_relocs = {}
for off in range(rela_param_sec["offset"], rela_param_sec["offset"] + rela_param_sec["size"], 24):
    r_offset = struct.unpack_from("<Q", elf_data, off)[0]
    r_info = struct.unpack_from("<Q", elf_data, off + 8)[0]
    r_addend = struct.unpack_from("<q", elf_data, off + 16)[0]
    r_sym = r_info >> 32
    r_type = r_info & 0xffffffff
    sym = symbols[r_sym]
    param_relocs[r_offset] = (sym["name"], r_addend)

print(f"Total param entries: {param_sec['size'] // 40}")
print(f"{'Idx':<4} {'Param Name':<30} {'Ops':<25} {'Arg (data off)':<15}")
print("-" * 75)

for idx in range(param_sec["size"] // 40):
    entry_off = idx * 40
    # name pointer at entry_off
    name_sym, name_addend = param_relocs.get(entry_off, (None, None))
    # ops pointer at entry_off + 16
    ops_sym, ops_addend = param_relocs.get(entry_off + 16, (None, None))
    # arg pointer at entry_off + 32
    arg_sym, arg_addend = param_relocs.get(entry_off + 32, (None, None))
    
    param_name = "UNKNOWN"
    if name_sym == ".rodata":
        # read from rodata
        str_start = rodata_sec["offset"] + name_addend
        str_end = elf_data.find(b"\0", str_start)
        param_name = elf_data[str_start:str_end].decode("ascii", errors="replace")
    elif name_sym:
        param_name = f"{name_sym}+{hex(name_addend)}"
        
    ops_str = f"{ops_sym}+{hex(ops_addend)}" if ops_sym else "None"
    arg_str = f"{arg_sym}+{hex(arg_addend)}" if arg_sym else "None"
    
    print(f"{idx:<4} {param_name:<30} {ops_str:<25} {arg_str:<15}")
