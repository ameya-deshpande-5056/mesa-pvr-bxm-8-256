import struct
import subprocess

# Let us find all occurrences of mbvnc0 and mbvnc1 in any section or text
# First get all symbols and sections

with open("firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko", "rb") as f:
    elf_data = f.read()

# Section rodata.str1.1 offset is 0xcb29c
# mbvnc0 is at file offset 0xd2c63, section rodata.str1.1 + 0x79c7
# mbvnc1 is at file offset 0xe7000, section rodata.str1.1 + 0x1bd64

# Let us check all relocations in all .rela sections pointing to rodata.str1.1 + 0x79c7 or near it
p = subprocess.Popen(["readelf", "-r", "firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko"], stdout=subprocess.PIPE, text=True)
stdout, _ = p.communicate()

print("Searching relocations for rodata.str1.1 + 79c7 (mbvnc0) and 1bd64 (mbvnc1)...")
for line in stdout.splitlines():
    if "rodata.str1.1" in line:
        if "79c7" in line or "1bd64" in line or "d8c6a" in line or "d9ce" in line:
            print(line)
