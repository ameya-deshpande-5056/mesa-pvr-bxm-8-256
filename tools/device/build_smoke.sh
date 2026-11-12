#!/bin/sh
# Builds the read-only enumeration binary (tools/smoke/vk_enum.c) for the phone.
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
out="$root/tools/device/vk_enum"

ndk=${ANDROID_NDK:-${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-}}}
if [ -z "$ndk" ]; then
   for candidate in "$HOME/Android/Sdk/ndk"/* "$HOME/android-ndk"*; do
      if [ -d "$candidate" ]; then
         ndk=$candidate
      fi
   done
fi

if [ -z "$ndk" ] || [ ! -d "$ndk" ]; then
   echo "error: set ANDROID_NDK to an NDK r25+ directory" >&2
   exit 1
fi

case "$(uname -s)" in
Linux) host=linux-x86_64 ;;
Darwin) host=darwin-x86_64 ;;
*) echo "error: unsupported build host $(uname -s)" >&2; exit 1 ;;
esac

cc="$ndk/toolchains/llvm/prebuilt/$host/bin/aarch64-linux-android29-clang"
[ -x "$cc" ] || { echo "error: no compiler at $cc" >&2; exit 1; }

"$cc" -O2 -Wall -Wextra -fPIE -pie \
   -o "$out" "$root/tools/smoke/vk_enum.c" -ldl

echo "built ${out#"$root"/}"
