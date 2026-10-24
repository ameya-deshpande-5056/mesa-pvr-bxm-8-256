import subprocess

p = subprocess.Popen(["aarch64-linux-gnu-objdump", "-d", "--start-address=0x8f864", "--stop-address=0x8fd60", "firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko"], stdout=subprocess.PIPE, text=True)
stdout, _ = p.communicate()

lines = stdout.splitlines()
cur_regs = {}
for i, line in enumerate(lines):
    # look for comparisons against x8
    if "cmp\tx8" in line or "mov" in line:
        pass
    if "cmp\tx8" in line:
        # print previous 5 lines
        for prev in lines[max(0, i-4):i+2]:
            print(prev)
        print("-" * 50)
