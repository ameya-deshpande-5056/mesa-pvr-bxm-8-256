# Device-side bring-up harness (M13/M14)

Everything in this directory is **gated** and **read-only with respect to the
device's graphics stack**. Nothing here flashes, writes to a system partition,
or talks to `/dev/pvr*` directly.

## Why it is gated

An earlier probe (`tools/pvr_connect_probe.c`) opened a bridge connection with
the wrong flag (`0x80` instead of bit 5, `0x20`). When the kernel driver tore
the connection down it dereferenced a string as a pointer and Oopsed in
`RemoveAndFreeStreamNode`, rebooting the phone. That file is frozen with
`#error`, and nothing here uses it.

The teardown path is still not understood, which is why every script below
refuses to run unless you explicitly authorize the session:

```sh
export PVR_DEVICE_AUTHORIZED=1
```

Without that variable the scripts exit immediately and run no `adb` command.
Setting it is you accepting that the phone may reboot.

## Steps

| Script | Milestone | What it does | Risk |
| :--- | :--- | :--- | :--- |
| `preflight.sh` | M13 | Read-only inventory: device model, API level, ABI, the existing Vulkan HALs, whether `/dev/pvr*` nodes exist, and the artifacts in `dist/`. | None: `getprop`/`ls` only. |
| `step2_smoke.sh` | M14/M15 | Builds `vk_enum`, pushes it plus the ICD JSON and driver `.so` into `/data/local/tmp/pvr-smoke/`, and runs it with `VK_ICD_FILENAMES` set. | Loads the driver and enumerates the device. No images, buffers, queues or command buffers. |
| `step3_device.sh` | M26 | Builds and pushes `vk_device`, which dlopens the driver directly (Android's loader ignores `VK_ICD_FILENAMES`, so the test is its own loader), creates instance, device, queue, a 64 KiB buffer with a 64 KiB UNIFORM descriptor (the M25 range), fills it on the GPU, reads the pattern back and tears everything down. | First GPU work through the driver. Submits one fill and waits. |

## Why the driver needs a render-node fallback

On this phone `drmGetDevices2()` returns nothing to an app- or shell-domain
process: libdrm reads `/sys/class/drm/<node>/device/uevent` to learn the bus
type, and SELinux denies that read (`avc: denied { read } for name="uevent"`).
The PowerVR render node is present and world-writable, and its DRM driver name
is `pvr` (25.1), so `pvr_instance.c` now falls back to opening
`/dev/dri/renderD128` when libdrm yields no compatible device.
`pvr_winsys_create()` still rejects any node whose DRM name is not `pvr` or
`powervr`, so the fallback cannot select a different GPU.

`vk_device --drm` prints the DRM driver name of every `/dev/dri` node, which is
the first thing to check when enumeration fails.

## Driver tracing

The driver is silent unless `PVR_DEBUG=srv` is set in its environment; that is
the single switch behind every bring-up trace (render-node selection, winsys
branch, each failing bridge call with its id, errno, and in/out sizes, and the
device-init stages). In an app such as Eden the variable is unset, so nothing is
written to stderr. `step3_device.sh` sets it and saves the output to
`device-evidence/vk_device-trace.txt`, which is where the bridge error code for
the v25 `DEVMEMINTCTXCREATE` call will show up.

## One command

```sh
export PVR_DEVICE_AUTHORIZED=1
tools/device/step2_smoke.sh
```

That is the whole of "load the driver and confirm Mesa's device appears". A
successful run ends with:

```
OK: 1 physical device(s) enumerated, no work submitted
```

and a device whose name is the Mesa PowerVR driver, not the vendor
`vulkan.mtk.so`.

## What it never does

- No `adb root`, no `adb remount`, no `adb shell su`.
- No writes outside `/data/local/tmp/pvr-smoke/`.
- No `fastboot`, no flashing, no partition access.
- No vendor `vulkaninfo`: `VK_ICD_FILENAMES` pins the loader to our ICD only,
  so a failure in our driver is not masked by the vendor one.

## Reading a failure

- `loader sees no devices`: the ICD JSON path or its `library_path` did not
  resolve. Both files go into the same directory and the JSON uses a relative
  `library_path`, so this usually means the `.so` did not finish pushing.
- A `dlopen` error naming `libvulkan_powervr_mesa.so`: the pushed `.so` does
  not match the device ABI, or is missing a dependency.
- A non-zero exit with no output: `adb` never reached the device. The script
  does not report success on a failed connection.

## Reverting

Nothing is installed. Delete the directory when done:

```sh
adb shell rm -rf /data/local/tmp/pvr-smoke
```
