#!/usr/bin/env python3
"""
generate_cross_file.py - Generates Meson cross file for Android AArch64.
Usage:
    python3 generate_cross_file.py --ndk <ndk_path> --native-bin <native_bin_dir> --out <output_path> [--api <api_level>]
"""

import os
import sys
import argparse

def main():
    parser = argparse.ArgumentParser(description="Generate Meson Android AArch64 cross file")
    parser.add_argument("--ndk", required=True, help="Path to Android NDK directory")
    parser.add_argument("--native-bin", required=True, help="Path to host native tools directory (mesa_clc, etc.)")
    parser.add_argument("--out", required=True, help="Path to write the cross.ini file")
    parser.add_argument("--api", default="34", help="Android API level (default: 34)")
    args = parser.parse_args()

    ndk_dir = os.path.abspath(args.ndk)
    llvm_bin = os.path.join(ndk_dir, "toolchains", "llvm", "prebuilt", "linux-x86_64", "bin")
    native_bin = os.path.abspath(args.native_bin)

    clang = os.path.join(llvm_bin, f"aarch64-linux-android{args.api}-clang")
    clangxx = os.path.join(llvm_bin, f"aarch64-linux-android{args.api}-clang++")
    ar = os.path.join(llvm_bin, "llvm-ar")
    strip = os.path.join(llvm_bin, "llvm-strip")

    content = f"""[binaries]
c = '{clang}'
cpp = '{clangxx}'
ar = '{ar}'
strip = '{strip}'
mesa_clc = '{os.path.join(native_bin, "mesa_clc")}'
vtn_bindgen2 = '{os.path.join(native_bin, "vtn_bindgen2")}'
pco_clc = '{os.path.join(native_bin, "pco_clc")}'
pdsc_precomp = '{os.path.join(native_bin, "pdsc_precomp")}'

[built-in options]
c_link_args = ['-static-libstdc++']
cpp_link_args = ['-static-libstdc++']

[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'armv8-a'
endian = 'little'
"""

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w") as f:
        f.write(content)

    print(f"[+] Successfully generated Android cross file at: {args.out}")

if __name__ == "__main__":
    main()

