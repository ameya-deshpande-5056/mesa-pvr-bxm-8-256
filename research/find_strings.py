import struct

with open("firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko", "rb") as f:
    elf_data = f.read()

e_shoff = struct.unpack_from("<Q", elf_data, 40)[0]
e_shentsize = struct.unpack_from("<H", elf_data, 58)[0]
e_shnum = struct.unpack_from("<H", elf_data, 60)[0]
e_shstrndx = struct.unpack_from("<H", elf_data, 62)[0]

shstrtab_hdr = elf_data[e_shoff + e_shstrndx * e_shentsize : e_shoff + (e_shstrndx + 1) * e_shentsize]
shstrtab_off = struct.unpack_from("<Q", shstrtab_hdr, 24)[0]

sections = []
for i in range(e_shnum):
    sh = elf_data[e_shoff + i * e_shentsize : e_shoff + (i + 1) * e_shentsize]
    sh_name_idx = struct.unpack_from("<I", sh, 0)[0]
    sh_type = struct.unpack_from("<I", sh, 4)[0]
    sh_offset = struct.unpack_from("<Q", sh, 24)[0]
    sh_size = struct.unpack_from("<Q", sh, 32)[0]
    
    name_end = elf_data.find(b"\0", shstrtab_off + sh_name_idx)
    name = elf_data[shstrtab_off + sh_name_idx : name_end].decode("ascii", errors="replace")
    sections.append({"index": i, "name": name, "offset": sh_offset, "size": sh_size, "type": sh_type})

for tgt_offset, label in [
    (0xd2c63, "mbvnc0"),
    (0xe7000, "mbvnc1"),
    (0xd8c6a, "_RGXBvncAcquireAppHint"),
    (0x1029f7, "RGXBVNC"),
    (0x102a6a, "IgnoreHWReportedBVNC")
]:
    for s in sections:
        if s["offset"] <= tgt_offset < s["offset"] + s["size"]:
            sec_offset = tgt_offset - s["offset"]
            sname = s["name"]
            sidx = s["index"]
            print(f"{label}: in section {sname} (index {sidx}) at section offset 0x{sec_offset:x} (file offset 0x{tgt_offset:x})")
