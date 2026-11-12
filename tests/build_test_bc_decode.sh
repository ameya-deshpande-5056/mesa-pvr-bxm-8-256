#!/bin/sh
# Builds the host unit test for the M23 BC emulation decode kernel.
#
# The generated util headers (u_format_gen.h, u_format_table.c, format_srgb.c)
# come from the existing Android cross-build tree; they are platform
# independent, which is what lets this run on the build host.
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
M=$root/research/mesa-src
F=$M/src/util/format

if [ ! -f "$M/build-android/src/util/format/u_format_table.c" ]; then
   echo "error: run the meson setup / first build of build-android first" >&2
   exit 1
fi

gcc -O2 -Wall -Wextra -pthread \
   -DHAVE_ENDIAN_H -DHAVE_PTHREAD -DHAVE_STRUCT_TIMESPEC -DHAVE_SYSCONF -DHAVE_SC_PHYS_PAGES \
   -I"$M/include" \
   -I"$M/src" \
   -I"$M/src/util" \
   -I"$M/src/util/format" \
   -I"$M/build-android/src" \
   -I"$M/build-android/src/util/format" \
   -I"$M/src/imagination/vulkan" \
   "$root/tests/test_bc_decode.c" \
   "$M/src/imagination/vulkan/pvr_bc_emulation.c" \
   "$F/u_format.c" \
   "$F/u_format_bptc.c" \
   "$F/u_format_etc.c" \
   "$F/u_format_fxt1.c" \
   "$F/u_format_latc.c" \
   "$F/u_format_other.c" \
   "$F/u_format_rgtc.c" \
   "$F/u_format_s3tc.c" \
   "$F/u_format_yuv.c" \
   "$F/u_format_zs.c" \
   "$M/src/util/rgtc.c" \
   "$M/src/util/half_float.c" \
   "$M/src/util/softfloat.c" \
   "$M/src/util/u_cpu_detect.c" \
   "$M/src/util/u_debug.c" \
   "$M/src/util/u_thread.c" \
   "$M/src/util/thread_sched.c" \
   "$M/src/util/os_file.c" \
   "$M/src/util/os_misc.c" \
   "$M/src/util/ralloc.c" \
   "$M/src/util/u_string.c" \
   "$M/src/util/u_printf.c" \
   "$M/src/util/hash_table.c" \
   "$M/src/util/blob.c" \
   "$M/src/util/u_call_once.c" \
   "$M/src/util/simple_mtx.c" \
   "$M/build-android/src/util/format_srgb.c" \
   "$M/build-android/src/util/format/u_format_table.c" \
   -lm \
   -o "$root/tests/test_bc_decode"

echo "built $root/tests/test_bc_decode"
