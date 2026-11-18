#!/usr/bin/env python3
"""Compare the DDK 25.1 bridge structs in pvr_srv_bridge_v25.h with the sizes
pvrsrvkm.ko actually registers for each bridge group/function.

BridgedDispatchKM rejects a call unless in_data_size and out_data_size are
exactly the registered values ("cmp w5, w6; b.ne" / "cmp w7, w8; b.ne"), so a
single wrong sizeof() is an EINVAL at the ioctl. This check catches that on the
host, with no phone attached.

Ground truth: the _SetDispatchTableEntry(...) registration sequence in the
vendor kernel module (firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko).

Run: python3 tools/verify_v25_bridge_abi.py
"""

import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(ROOT)
KO = os.path.join(REPO, "firmware/vendor-dlkm-files/lib/modules/pvrsrvkm.ko")
HDR = os.path.join(REPO, "research/mesa-src/src/imagination/vulkan/winsys/"
                          "pvrsrvkm/pvr_srv_bridge_v25.h")

OBJDUMP = "aarch64-linux-gnu-objdump"
READELF = "aarch64-linux-gnu-readelf"

# (bridge group, function symbol) -> (in struct or None, out struct or None)
# Every entry is a call the driver makes through pvr_srv_bridge_call().
CALLS = [
    (1, "PVRSRV_BRIDGE_SRVCORE_CONNECT",
     "pvr_srv_bridge_connect_in_v25", "pvr_srv_bridge_connect_out_v25"),
    (1, "PVRSRV_BRIDGE_SRVCORE_DISCONNECT",
     None, "pvr_srv_bridge_disconnect_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_PMRUNEXPORTPMR",
     "pvr_srv_bridge_pmr_unref_pmr_in_v25",
     "pvr_srv_bridge_pmr_unref_pmr_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_PMRUNREFPMR",
     "pvr_srv_bridge_pmr_unref_pmr_in_v25",
     "pvr_srv_bridge_pmr_unref_pmr_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDPMR",
     "pvr_srv_bridge_physmem_new_ram_backed_pmr_in_v25",
     "pvr_srv_bridge_physmem_new_ram_backed_pmr_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_DEVMEMINTCTXCREATE",
     "pvr_srv_bridge_devmem_int_ctx_create_in_v25",
     "pvr_srv_bridge_devmem_int_ctx_create_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_DEVMEMINTCTXDESTROY",
     "pvr_srv_bridge_devmem_int_ctx_destroy_in_v25",
     "pvr_srv_bridge_devmem_int_ctx_destroy_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_DEVMEMINTHEAPCREATE",
     "pvr_srv_bridge_devmem_int_heap_create_in_v25",
     "pvr_srv_bridge_devmem_int_heap_create_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_DEVMEMINTHEAPDESTROY",
     "pvr_srv_bridge_devmem_int_heap_destroy_in_v25",
     "pvr_srv_bridge_devmem_int_heap_destroy_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_HEAPCFGHEAPCONFIGCOUNT",
     None, "pvr_srv_bridge_heap_cfg_heap_config_count_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_HEAPCFGHEAPCOUNT",
     "pvr_srv_bridge_heap_cfg_heap_count_in_v25",
     "pvr_srv_bridge_heap_cfg_heap_count_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_HEAPCFGHEAPDETAILS",
     "pvr_srv_bridge_heap_cfg_heap_details_in_v25",
     "pvr_srv_bridge_heap_cfg_heap_details_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_DEVMEMXINTRESERVERANGE",
     "pvr_srv_bridge_devmemx_int_reserve_range_in_v25",
     "pvr_srv_bridge_devmemx_int_reserve_range_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_DEVMEMXINTUNRESERVERANGE",
     "pvr_srv_bridge_devmemx_int_unreserve_range_in_v25",
     "pvr_srv_bridge_devmemx_int_unreserve_range_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_DEVMEMXINTMAPPAGES",
     "pvr_srv_bridge_devmemx_int_map_pages_in_v25",
     "pvr_srv_bridge_devmemx_int_map_pages_out_v25"),
    (6, "PVRSRV_BRIDGE_MM_DEVMEMXINTUNMAPPAGES",
     "pvr_srv_bridge_devmemx_int_unmap_pages_in_v25",
     "pvr_srv_bridge_devmemx_int_unmap_pages_out_v25"),
    (11, "PVRSRV_BRIDGE_DMABUF_PHYSMEMEXPORTDMABUF",
     "pvr_srv_bridge_physmem_export_dmabuf_in_v25",
     "pvr_srv_bridge_physmem_export_dmabuf_out_v25"),
    (129, "PVRSRV_BRIDGE_RGXCMP_RGXCREATECOMPUTECONTEXT",
     "pvr_srv_bridge_rgx_create_compute_context_in_v25",
     "pvr_srv_bridge_rgx_create_compute_context_out_v25"),
    (129, "PVRSRV_BRIDGE_RGXCMP_RGXDESTROYCOMPUTECONTEXT",
     "pvr_srv_bridge_rgx_destroy_compute_context_in_v25",
     "pvr_srv_bridge_rgx_destroy_compute_context_out_v25"),
    (129, "PVRSRV_BRIDGE_RGXCMP_RGXKICKCDM",
     "pvr_srv_bridge_rgx_kick_cdm_in_v25",
     "pvr_srv_bridge_rgx_kick_cdm_out_v25"),
]


def kernel_table():
    """Return {(bridge_id, func_id): (in_size, out_size, name)} from the .ko."""
    dis = subprocess.check_output([OBJDUMP, "-d", KO]).decode().splitlines()
    ins = []
    for line in dis:
        m = re.match(r"^\s*([0-9a-f]+):\s+([0-9a-f]+)\s+(\S+)\s*(.*)$", line)
        if m:
            ins.append((int(m.group(1), 16), m.group(3),
                        re.sub(r"\s*//.*$", "", m.group(4)).strip()))

    relocs = {}
    for line in subprocess.check_output([READELF, "-W", "-r", KO]).decode().splitlines():
        parts = line.split()
        if len(parts) >= 5:
            try:
                relocs[int(parts[0], 16)] = line
            except ValueError:
                pass

    strings = None
    for line in subprocess.check_output([READELF, "-W", "-S", KO]).decode().splitlines():
        if "PROGBITS" in line and ".rodata.str1.1" in line:
            parts = line.split("] ")[-1].split()
            off, size = int(parts[3], 16), int(parts[4], 16)
            strings = open(KO, "rb").read()[off:off + size]
    if strings is None:
        sys.exit("error: .rodata.str1.1 not found in pvrsrvkm.ko")

    def string_at(off):
        end = strings.find(b"\0", off)
        return strings[off:end].decode("latin1")

    regs = {}
    out_size = None
    name = None
    table = {}
    for addr, mnem, op in ins:
        m = re.match(r"^(w\d+), (#(?:0x[0-9a-f]+|\d+)|wzr)$", op)
        if mnem == "mov" and m:
            regs[m.group(1)] = (0 if m.group(2) == "wzr"
                                else int(m.group(2).lstrip("#"), 0))
            continue
        m = re.match(r"^(w\d+), \[sp\]$", op)
        if mnem == "str" and m:
            out_size = regs.get(m.group(1))
            continue
        if op.startswith("x2,"):
            r = relocs.get(addr)
            if r and ".rodata.str1.1 +" in r:
                name = string_at(int(r.split("+ ")[-1], 16))
        if mnem == "bl" and "_SetDispatchTableEntry" in op:
            key = (regs.get("w0"), regs.get("w1"))
            if key[0] is not None and key[1] is not None and name:
                table[key] = (regs.get("w7"), out_size, name)
    return table


def header_sizes():
    """Return {struct_name: size} from the header's sizeof static asserts."""
    text = open(HDR).read()
    return {m.group(1): int(m.group(2))
            for m in re.finditer(
                r"_Static_assert\(sizeof\(struct (\w+)\) == (\d+),", text)}


def main():
    table = kernel_table()
    sizes = header_sizes()
    failures = 0

    print("=" * 78)
    print("DDK 25.1 bridge ABI: pvr_srv_bridge_v25.h vs pvrsrvkm.ko")
    print("=" * 78)
    for group, func_name, in_struct, out_struct in CALLS:
        entry = None
        for (gid, fid), value in table.items():
            if gid == group and value[2] == func_name:
                entry = (fid, value)
                break
        if entry is None:
            print(f"FAIL {func_name}: not registered in pvrsrvkm.ko")
            failures += 1
            continue
        fid, (k_in, k_out, _) = entry

        want_in = 0 if in_struct is None else sizes.get(in_struct)
        want_out = 0 if out_struct is None else sizes.get(out_struct)
        ok = (want_in == k_in and want_out == k_out)
        if not ok:
            failures += 1
        print(f"{'PASS' if ok else 'FAIL'} bridge {group:>3} func {fid:>3} "
              f"{func_name:<46} kernel {str(k_in):>3}/{str(k_out):>3} "
              f"header {str(want_in):>3}/{str(want_out):>3}")

    print("=" * 78)
    if failures:
        print(f"{failures} bridge call(s) disagree with pvrsrvkm.ko")
        return 1
    print(f"all {len(CALLS)} bridge calls match the registered sizes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
