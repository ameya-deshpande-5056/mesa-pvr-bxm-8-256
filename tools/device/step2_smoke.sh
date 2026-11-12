#!/bin/sh
# Step 2, one command: load the driver and confirm Mesa's device appears.
#
# This is M14 (app-private deployment) + the M15 enumeration check, and it is
# purely read-only with respect to the graphics stack: no images, buffers,
# queues or command buffers, and no submission. It only writes inside
# /data/local/tmp/pvr-smoke/.
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
device_dir=/data/local/tmp/pvr-smoke
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

if [ "${PVR_DEVICE_AUTHORIZED:-}" != "1" ]; then
   cat >&2 <<'EOF'
REFUSING TO RUN: device access is not authorized.

Read tools/device/README.md first. The teardown path that caused the
RemoveAndFreeStreamNode kernel Oops is still not understood. If you accept that
the phone may reboot, re-run with:

   export PVR_DEVICE_AUTHORIZED=1
   tools/device/step2_smoke.sh
EOF
   exit 2
fi

"$root/tools/device/preflight.sh" || exit 1

zip="$root/dist/vulkan_pvr_bxm8256_icd.zip"
[ -f "$zip" ] || { echo "error: missing $zip (run tools/package_driver.py)" >&2; exit 1; }

echo
echo "== staging artifacts =="
unzip -o -q "$zip" -d "$stage"
ls -l "$stage"

if [ ! -x "$root/tools/device/vk_enum" ]; then
   echo
   echo "== building vk_enum =="
   "$root/tools/device/build_smoke.sh"
fi

echo
echo "== pushing to $device_dir =="
adb shell "mkdir -p $device_dir"
adb push "$stage/powervr_icd.aarch64.json" "$device_dir/" >/dev/null
adb push "$stage/libvulkan_powervr_mesa.so" "$device_dir/" >/dev/null
adb push "$root/tools/device/vk_enum" "$device_dir/" >/dev/null
adb shell "chmod 755 $device_dir/vk_enum"

echo
echo "== running read-only enumeration with our ICD only =="
set +e
adb shell "cd $device_dir && \
   VK_ICD_FILENAMES=$device_dir/powervr_icd.aarch64.json \
   VK_LOADER_DEBUG=error,driver \
   ./vk_enum"
status=$?
set -e

echo
if [ "$status" -eq 0 ]; then
   echo "PASS: Mesa's PowerVR device enumerated (step 2 complete)"
   echo "      artifacts left in $device_dir; remove with:"
   echo "      adb shell rm -rf $device_dir"
else
   echo "FAIL: enumeration exited $status -- see the loader output above" >&2
fi

exit "$status"
