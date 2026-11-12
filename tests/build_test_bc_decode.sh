#!/bin/sh
# Builds the host unit test for the M23 BC emulation decode kernel.
#
# The test needs the generated util/format files (u_format_gen.h,
# u_format_pack.h, u_format_table.c, format_srgb.c). They are platform
# independent, so where they come from does not matter:
#
#   1. $MESA_BUILD_DIR, if it points at a configured build tree;
#   2. research/mesa-src/build-android, the Android cross-build;
#   3. otherwise Mesa's own generators, run here into build-bc-test/.
#
# Case 3 is what lets CI run this without a full meson setup.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
M=$root/research/mesa-src
F=$M/src/util/format

if [ ! -f "$F/u_format_table.py" ]; then
   echo "error: mesa-src is not initialised (git submodule update --init)" >&2
   exit 1
fi

if [ -n "${MESA_BUILD_DIR:-}" ] && [ -f "$MESA_BUILD_DIR/src/util/format/u_format_table.c" ]; then
   GEN=$MESA_BUILD_DIR
elif [ -f "$M/build-android/src/util/format/u_format_table.c" ]; then
   GEN=$M/build-android
else
   GEN=$root/build-bc-test
   echo "generating util/format sources into ${GEN#"$root"/}"
   mkdir -p "$GEN/src/util/format"
   python3 "$F/u_format_table.py" "$F/u_format.yaml" --enums \
      > "$GEN/src/util/format/u_format_gen.h"
   python3 "$F/u_format_table.py" "$F/u_format.yaml" --header \
      > "$GEN/src/util/format/u_format_pack.h"
   python3 "$F/u_format_table.py" "$F/u_format.yaml" \
      > "$GEN/src/util/format/u_format_table.c"
   python3 "$M/src/util/format_srgb.py" \
      > "$GEN/src/util/format_srgb.c"
fi

gcc -O2 -Wall -Wextra -pthread \
   -DHAVE_ENDIAN_H -DHAVE_PTHREAD -DHAVE_STRUCT_TIMESPEC -DHAVE_SYSCONF -DHAVE_SC_PHYS_PAGES \
   -I"$M/include" \
   -I"$M/src" \
   -I"$M/src/util" \
   -I"$M/src/util/format" \
   -I"$GEN/src" \
   -I"$GEN/src/util/format" \
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
   "$GEN/src/util/format_srgb.c" \
   "$GEN/src/util/format/u_format_table.c" \
   -lm \
   -o "$root/tests/test_bc_decode"

echo "built ${root#"$PWD"/}/tests/test_bc_decode (generated headers: ${GEN#"$root"/})"
