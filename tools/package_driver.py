#!/usr/bin/env python3
"""
package_driver.py - Validates and packages the PowerVR BXM-8-256 Mesa Vulkan driver
for adrenotools (Eden / Azahar) and standalone ICD loaders (Winlator / Gamehub).
"""

import os
import sys
import json
import zipfile
import subprocess
import shutil

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BUILD_DIR = os.path.join(REPO_ROOT, "research", "mesa-src", "build-android")
DRIVER_SO = os.path.join(BUILD_DIR, "src", "imagination", "vulkan", "libvulkan_powervr_mesa.so")
DIST_DIR = os.path.join(REPO_ROOT, "dist")

REQUIRED_EXPORTS = [
    "HMI",
    "vk_icdGetInstanceProcAddr",
    "vk_icdGetPhysicalDeviceProcAddr",
    "vk_icdNegotiateLoaderICDInterfaceVersion"
]

ALLOWED_SYSTEM_LIBS = {
    "libc.so",
    "libm.so",
    "libdl.so",
    "libz.so",
    "liblog.so",
    "libhardware.so",
    "libnativewindow.so",
    "libsync.so"
}

def check_binary():
    if not os.path.isfile(DRIVER_SO):
        print(f"[-] Driver binary not found: {DRIVER_SO}")
        sys.exit(1)

    print(f"[+] Found driver binary: {DRIVER_SO} ({os.path.getsize(DRIVER_SO):,} bytes)")

    # Verify exported symbols
    nm_proc = subprocess.run(
        ["aarch64-linux-gnu-nm", "-D", DRIVER_SO],
        capture_output=True, text=True, check=True
    )
    exported_symbols = set()
    for line in nm_proc.stdout.splitlines():
        parts = line.strip().split()
        if len(parts) >= 3 and parts[1] in ("T", "D", "B", "R"):
            exported_symbols.add(parts[2])

    print("[*] Checking required exported symbols...")
    for sym in REQUIRED_EXPORTS:
        if sym in exported_symbols:
            print(f"  [OK] Exported: {sym}")
        else:
            print(f"  [FAIL] Missing required symbol: {sym}")
            sys.exit(1)

    # Verify DT_NEEDED
    readelf_proc = subprocess.run(
        ["aarch64-linux-gnu-readelf", "-d", DRIVER_SO],
        capture_output=True, text=True, check=True
    )
    needed = []
    for line in readelf_proc.stdout.splitlines():
        if "(NEEDED)" in line and "Shared library:" in line:
            lib = line.split("[")[1].split("]")[0]
            needed.append(lib)

    print("[*] Checking DT_NEEDED dynamic dependencies...")
    for lib in needed:
        if lib in ALLOWED_SYSTEM_LIBS:
            print(f"  [OK] System dependency: {lib}")
        else:
            print(f"  [FAIL] Disallowed or non-system dependency: {lib}")
            sys.exit(1)

def build_packages():
    os.makedirs(DIST_DIR, exist_ok=True)

    # 1. Adrenotools Package (Flat zip with meta.json and .so)
    meta = {
        "schemaVersion": 1,
        "name": "PowerVR BXM-8-256 Mesa",
        "description": "Mesa vulkan_pvr driver for PowerVR B-Series BXM-8-256 (BVNC 35.3.408.101)",
        "author": "Mesa / BXM Research",
        "packageVersion": "1",
        "vendor": "Imagination",
        "driverVersion": "Mesa 27.0.0-devel",
        "minApi": 29,
        "libraryName": "libvulkan_powervr_mesa.so"
    }

    adrenotools_zip_path = os.path.join(DIST_DIR, "vulkan_pvr_bxm8256_adrenotools.zip")
    with zipfile.ZipFile(adrenotools_zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("meta.json", json.dumps(meta, indent=2))
        z.write(DRIVER_SO, arcname="libvulkan_powervr_mesa.so")

    print(f"[+] Created Adrenotools package: {adrenotools_zip_path} ({os.path.getsize(adrenotools_zip_path):,} bytes)")

    # 2. Standalone Vulkan ICD Package (ICD JSON manifest + .so)
    icd_manifest = {
        "file_format_version": "1.0.1",
        "ICD": {
            "library_path": "libvulkan_powervr_mesa.so",
            "api_version": "1.4.363",
            "library_arch": "64"
        }
    }

    icd_dir = os.path.join(DIST_DIR, "standalone_icd")
    os.makedirs(icd_dir, exist_ok=True)
    manifest_path = os.path.join(icd_dir, "powervr_icd.aarch64.json")
    with open(manifest_path, "w") as f:
        json.dump(icd_manifest, f, indent=4)
        f.write("\n")
    shutil.copy2(DRIVER_SO, os.path.join(icd_dir, "libvulkan_powervr_mesa.so"))

    icd_zip_path = os.path.join(DIST_DIR, "vulkan_pvr_bxm8256_icd.zip")
    with zipfile.ZipFile(icd_zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(manifest_path, arcname="powervr_icd.aarch64.json")
        z.write(DRIVER_SO, arcname="libvulkan_powervr_mesa.so")

    print(f"[+] Created Standalone ICD package: {icd_zip_path} ({os.path.getsize(icd_zip_path):,} bytes)")
    print(f"[+] ICD manifest created at: {manifest_path}")

if __name__ == "__main__":
    check_binary()
    build_packages()
