#!/bin/sh
# Step 3: the first real GPU work through the BXM-8-256 driver.
#
# Unlike step2_smoke.sh this creates a VkDevice, allocates memory, binds a
# 64 KiB uniform descriptor (M25), fills the buffer on the GPU, waits, reads
# the pattern back and tears everything down. It exercises the pvrsrvkm bridge,
# the allocator and the command-stream submit/teardown path -- the same code the
# emulator will hit -- and it keeps the state it touches inside
# /data/local/tmp/pvr-smoke/ plus a log in device-evidence/.
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
device_dir=/data/local/tmp/pvr-smoke
log="$root/device-evidence/vk_device-log.txt"
trace="$root/device-evidence/vk_device-trace.txt"
stage=$(mktemp -d)
logcat_pid=

cleanup() {
   if [ -n "$logcat_pid" ]; then
      kill "$logcat_pid" 2>/dev/null || true
      wait "$logcat_pid" 2>/dev/null || true
      logcat_pid=
   fi
   rm -rf "$stage"
}
trap cleanup EXIT

if [ "${PVR_DEVICE_AUTHORIZED:-}" != "1" ]; then
   cat >&2 <<'EOF'
REFUSING TO RUN: device access is not authorized.

This step submits GPU work for the first time through the new driver. Read
tools/device/README.md first; if you accept that the phone may reboot, run:

   export PVR_DEVICE_AUTHORIZED=1
   tools/device/step3_device.sh
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

if [ ! -x "$root/tools/device/vk_enum" ] || [ ! -x "$root/tools/device/vk_device" ]; then
   echo
   echo "== building smoke binaries =="
   "$root/tools/device/build_smoke.sh"
fi

echo
echo "== pushing to $device_dir =="
adb shell "mkdir -p $device_dir"
adb push "$stage/powervr_icd.aarch64.json" "$device_dir/" >/dev/null
adb push "$stage/libvulkan_powervr_mesa.so" "$device_dir/" >/dev/null
adb push "$root/tools/device/vk_device" "$device_dir/" >/dev/null
adb shell "chmod 755 $device_dir/vk_device"

mkdir -p "$root/device-evidence"
adb logcat -c 2>/dev/null || true
adb logcat -b all -v time >"$log" 2>&1 &
logcat_pid=$!

echo
echo "== running vk_device against our ICD =="
# PVR_DEBUG=srv enables the driver's single opt-in bring-up trace; without it
# the driver writes nothing to stderr.
set +e
adb shell "cd $device_dir && PVR_DEBUG=srv LD_LIBRARY_PATH=$device_dir ./vk_device $device_dir/libvulkan_powervr_mesa.so" >"$trace" 2>&1
status=$?
set -e
cat "$trace"

sleep 1
echo
echo "== kernel health =="
adb logcat -b kernel -d 2>/dev/null | grep -E "Oops|panic|watchdog|BUG:" | tail -5 || true
echo "(no lines above means no Oops/panic/watchdog in the kernel buffer)"

echo
if [ "$status" -eq 0 ]; then
   echo "PASS: device, submit and readback all worked (step 3 complete)"
else
   echo "FAIL: vk_device exited $status -- driver output is in ${log#"$root"/}"
fi
echo "logs:     ${log#"$root"/}"
echo "trace:    ${trace#"$root"/}"
echo "clean up: adb shell rm -rf $device_dir"

exit "$status"
