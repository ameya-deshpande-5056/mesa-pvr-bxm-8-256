#!/bin/sh
# M13 pre-flight gate: read-only inventory of the device and the artifacts.
#
# Runs no writes and no graphics calls: getprop/ls only. Still gated, because
# a mistyped command here would be a device command, and the project's policy
# is that device access is explicit.
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)

if [ "${PVR_DEVICE_AUTHORIZED:-}" != "1" ]; then
   cat >&2 <<'EOF'
REFUSING TO RUN: device access is not authorized.

The teardown path that caused the RemoveAndFreeStreamNode kernel Oops is still
not understood, so device commands are opt-in. If you accept that the phone may
reboot, re-run with:

   export PVR_DEVICE_AUTHORIZED=1
   tools/device/preflight.sh
EOF
   exit 2
fi

fail() {
   echo "FAIL: $*" >&2
   exit 1
}

command -v adb >/dev/null 2>&1 || fail "adb is not on PATH"

state=$(adb get-state 2>&1) || fail "adb get-state: $state"
[ "$state" = "device" ] || fail "no device in 'device' state (got '$state')"

echo "== device =="
adb shell getprop ro.product.model
adb shell getprop ro.product.device
adb shell getprop ro.build.version.sdk
adb shell getprop ro.product.cpu.abi

echo
echo "== Vulkan HALs already present =="
adb shell 'ls -1 /vendor/lib64/hw 2>/dev/null | grep -i vulkan || echo "(none)"'
adb shell 'ls -1 /vendor/lib64 2>/dev/null | grep -i "libvulkan\|vulkan.mtk" || echo "(none)"'

echo
echo "== PowerVR kernel interface (read-only listing) =="
adb shell 'ls -l /dev/pvr* 2>/dev/null || echo "(no /dev/pvr* nodes)"'
adb shell 'ls -1 /vendor/lib/modules 2>/dev/null | grep -i pvr || echo "(no pvrsrvkm module listing)"'

echo
echo "== staging directory =="
adb shell 'ls -l /data/local/tmp 2>/dev/null | head -20'

echo
echo "== host artifacts =="
for f in "$root/dist/vulkan_pvr_bxm8256_icd.zip" \
         "$root/dist/vulkan_pvr_bxm8256_adrenotools.zip"; do
   if [ -f "$f" ]; then
      echo "  present: ${f#"$root"/} ($(wc -c <"$f") bytes)"
   else
      echo "  MISSING: ${f#"$root"/} (run tools/package_driver.py)"
   fi
done

echo
echo "PASS: device reachable and read-only inventory collected"
echo "      next: tools/device/step2_smoke.sh"
