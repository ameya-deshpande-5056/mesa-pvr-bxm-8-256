#!/usr/bin/env python3
"""
M23 gate: BC emulation image path wiring.

The BC emulation path is not exercised by any on-device sample yet, so the one
thing that must hold is that it stays wired together *and* stays unadvertised.
These are static checks over the driver sources; they catch the two failure
modes that matter right now:

  - the feature getting advertised before an end-to-end BC workload works, and
  - the plumbing drifting apart (create substituting a format that views or
    buffer copies do not follow), which would render garbage instead of failing.

Paths are derived from this file so the gate also runs from a CI checkout.
"""

import os
import sys

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
MESA_SRC = os.path.join(REPO_ROOT, "research/mesa-src")
if not os.path.isdir(os.path.join(MESA_SRC, "src/imagination/vulkan")):
    print("SKIP: mesa-src is not initialised (git submodule update --init)")
    sys.exit(1)

VK_DIR = os.path.join(MESA_SRC, "src/imagination/vulkan")

checks = []


def read(rel_path):
    with open(os.path.join(VK_DIR, rel_path)) as f:
        return f.read()


def check(name, condition, detail=""):
    checks.append((name, condition, detail))


def main():
    physical = read("pvr_physical_device.c")
    emulation_c = read("pvr_bc_emulation.c")
    image_c = read("pvr_image.c")
    arch_image_c = read("pvr_arch_image.c")
    blit_c = read("rogue/pvr_blit.c")

    # 1. The feature must not be advertised yet.
    check(
        "textureCompressionBC is not advertised",
        ".textureCompressionBC = false," in physical,
        "pvr_physical_device.c must keep .textureCompressionBC = false",
    )

    # 2. Image creation substitutes the uncompressed format.
    check(
        "create substitutes the emulated format",
        "pvr_bc_emulation_create_info" in image_c
        and "emulated_info->format = pvr_bc_emulated_vk_format(bc);" in image_c,
    )
    check(
        "create stores the emulation description",
        "image->bc_emulated = true;" in image_c and "image->bc = bc;" in image_c,
    )
    check(
        "create passes the emulated info to image init",
        "pvr_image_init(device, create_info, image);" in image_c,
    )

    # 3. BC is refused while the feature is off (spec requires this).
    check(
        "unsupported BC create fails",
        "VK_ERROR_FORMAT_NOT_SUPPORTED" in image_c
        and "pvr_bc_emulation_allowed" in image_c,
    )
    check(
        "path is reachable for bring-up",
        'debug_get_bool_option("PVR_BC_EMULATION", false)' in image_c,
    )

    # 4. Views follow the image format, otherwise sampling reads the wrong thing.
    check(
        "views use the emulated format",
        "emulated_info.format = pvr_bc_emulated_vk_format(&image->bc)"
        in arch_image_c,
    )
    check(
        "texture state uses the view format",
        "info.format = iview->vk.format;" in arch_image_c,
    )

    # 5. Buffer -> image copies decode compressed blocks.
    check(
        "BC buffer copies decode",
        "pvr_bc_decode_level(bc," in blit_c
        and "pvr_bc_copy_buffer_to_image_region(cmd_buffer, src, dst, region)"
        in blit_c,
    )
    check(
        "decode is gated on the emulated image",
        "if (dst->bc_emulated) {" in blit_c,
    )
    staging_copy = blit_c.find("staging_bo->dev_addr")
    check(
        "decoded data is uploaded before the transfer",
        staging_copy != -1
        and blit_c.index("pvr_arch_cmd_buffer_upload_general") < staging_copy,
    )

    # 6. BC6H must declare 4-byte channels, matching the bptc decoder's float
    # output; a narrower target under-sizes every buffer sized from it.
    check(
        "BC6H target is 128 bpp",
        "PVR_BC_TARGET_RGBA32F, 16, 128" in emulation_c
        and "PVR_BC_TARGET_RGBA16F" not in emulation_c,
        "BC6H must target RGBA32F with target_bpp 128",
    )

    # 7. The format table and its VkFormat mapping agree.
    for target, vk_format in (
        ("PVR_BC_TARGET_RGBA8_UNORM", "VK_FORMAT_R8G8B8A8_UNORM"),
        ("PVR_BC_TARGET_RGBA8_SRGB", "VK_FORMAT_R8G8B8A8_SRGB"),
        ("PVR_BC_TARGET_RGBA32F", "VK_FORMAT_R32G32B32A32_SFLOAT"),
    ):
        check(
            f"{target} maps to {vk_format}",
            f"case {target}:" in emulation_c
            and emulation_c.split(f"case {target}:")[1].lstrip().startswith(
                f"return {vk_format};"
            ),
        )

    failed = 0
    for name, ok, detail in checks:
        print(f"  {'PASS' if ok else 'FAIL'}: {name}")
        if not ok:
            if detail:
                print(f"        {detail}")
            failed += 1

    print(f"\nCheckpoint: {len(checks) - failed}/{len(checks)} checks passed")
    if failed:
        print("  ❌ M23 BC image path wiring is broken")
        return 1
    print("  ✅ M23 BC image path is wired and unadvertised")
    return 0


if __name__ == "__main__":
    sys.exit(main())
