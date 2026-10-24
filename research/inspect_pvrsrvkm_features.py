import struct

with open("firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko", "rb") as f:
    elf = f.read()

# .rodata file offset = 0x101970
rodata_off = 0x101970
table_off = 0xff98

# In RGXBvncInitialiseConfiguration:
# 8fb18: add x11, x8, #0x8
# 8fb2c: str x11, [x26, #16] -> feature table pointer?
# 8fb30: add x11, x8, #0x18
# 8fb3c: add x8, x26, #0x28
# Let us dump 128 bytes at .rodata + 0xff98
slice_data = elf[rodata_off + table_off : rodata_off + table_off + 256]
for i in range(0, 256, 16):
    hex_str = " ".join(f"{b:02x}" for b in slice_data[i:i+16])
    print(f"+0x{i:02x} (.rodata+0x{table_off+i:04x}): {hex_str}")
