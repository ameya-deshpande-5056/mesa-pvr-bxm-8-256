/*
 * Host unit test for the M23 BC emulation decode kernel.
 *
 * No GPU and no Vulkan driver are involved: this decodes known BC blocks and
 * checks exact texels, so a wiring mistake in pvr_bc_emulation.c is caught on
 * the build host rather than by a game rendering garbage on the phone.
 *
 * Build and run:
 *   gcc -O2 -Wall -Wextra \
 *       -I research/mesa-src/include -I research/mesa-src/src \
 *       -I research/mesa-src/src/imagination/vulkan \
 *       tests/test_bc_decode.c \
 *       research/mesa-src/src/imagination/vulkan/pvr_bc_emulation.c \
 *       research/mesa-src/src/util/format/u_format_s3tc.c \
 *       research/mesa-src/src/util/format/u_format_bptc.c \
 *       research/mesa-src/src/util/format/u_format_rgtc.c \
 *       -o tests/test_bc_decode
 *   ./tests/test_bc_decode
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pvr_bc_emulation.h"

static int pass_count;
static int fail_count;

static void
check(bool cond, const char *what)
{
   if (cond) {
      pass_count++;
      printf("  PASS: %s\n", what);
   } else {
      fail_count++;
      printf("  FAIL: %s\n", what);
   }
}

static void
check_pixel(const uint8_t *img, unsigned stride, unsigned x, unsigned y,
            uint8_t r, uint8_t g, uint8_t b, uint8_t a, const char *what)
{
   const uint8_t *p = img + (size_t)y * stride + (size_t)x * 4;
   const bool ok = p[0] == r && p[1] == g && p[2] == b && p[3] == a;

   if (!ok) {
      printf("         pixel (%u,%u) = %u,%u,%u,%u (expected %u,%u,%u,%u)\n",
             x, y, p[0], p[1], p[2], p[3], r, g, b, a);
   }
   check(ok, what);
}

/* A BC colour block: c0 = pure red, c1 = pure blue, all indices 0.
 *
 * BC1 uses 4-colour mode when c0 > c1, so index 0 selects c0 exactly, giving
 * an unambiguous expected texel instead of an interpolation.
 */
static void
make_bc_color_block(uint8_t *out, bool bc1)
{
   const uint16_t c0 = 0xF800; /* R5 = 31 -> 255, G6 = 0, B5 = 0 */
   const uint16_t c1 = 0x001F; /* B5 = 31 -> 255 */

   if (!bc1) {
      /* BC2/BC3 put the 64-bit alpha block (or BC2 explicit alpha) first.
       * 0xFF = fully opaque for BC3; 0xFFFF... = fully opaque for BC2.
       */
      memset(out, 0xFF, 8);
      out += 8;
   }

   out[0] = c0 & 0xFF;
   out[1] = c0 >> 8;
   out[2] = c1 & 0xFF;
   out[3] = c1 >> 8;
   memset(out + 4, 0x00, 4); /* 16 indices, all zero */
}

int
main(void)
{
   printf("== M23 BC emulation decode kernel ==\n");

   printf("\n[1] Format mapping\n");
   struct pvr_bc_emulation_info info;
   static const struct {
      VkFormat format;
      enum pvr_bc_target_format target;
      uint8_t block_bytes;
   } expected[] = {
      { VK_FORMAT_BC1_RGB_UNORM_BLOCK, PVR_BC_TARGET_RGBA8_UNORM, 8 },
      { VK_FORMAT_BC1_RGB_SRGB_BLOCK, PVR_BC_TARGET_RGBA8_SRGB, 8 },
      { VK_FORMAT_BC1_RGBA_UNORM_BLOCK, PVR_BC_TARGET_RGBA8_UNORM, 8 },
      { VK_FORMAT_BC2_UNORM_BLOCK, PVR_BC_TARGET_RGBA8_UNORM, 16 },
      { VK_FORMAT_BC3_UNORM_BLOCK, PVR_BC_TARGET_RGBA8_UNORM, 16 },
      { VK_FORMAT_BC3_SRGB_BLOCK, PVR_BC_TARGET_RGBA8_SRGB, 16 },
      { VK_FORMAT_BC4_UNORM_BLOCK, PVR_BC_TARGET_RGBA8_UNORM, 8 },
      { VK_FORMAT_BC5_UNORM_BLOCK, PVR_BC_TARGET_RGBA8_UNORM, 16 },
      { VK_FORMAT_BC6H_UFLOAT_BLOCK, PVR_BC_TARGET_RGBA16F, 16 },
      { VK_FORMAT_BC7_UNORM_BLOCK, PVR_BC_TARGET_RGBA8_UNORM, 16 },
      { VK_FORMAT_BC7_SRGB_BLOCK, PVR_BC_TARGET_RGBA8_SRGB, 16 },
   };

   for (unsigned i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
      char what[128];
      const bool found = pvr_bc_format_info(expected[i].format, &info);

      snprintf(what, sizeof(what), "format %d maps (block %u bytes)",
               (int)expected[i].format, expected[i].block_bytes);
      check(found && info.target == expected[i].target &&
               info.block_bytes == expected[i].block_bytes &&
               info.block_width == 4 && info.block_height == 4,
            what);
   }

   check(!pvr_bc_format_info(VK_FORMAT_R8G8B8A8_UNORM, &info),
         "linear formats are not mapped");
   check(pvr_bc_format_is_compressed(VK_FORMAT_BC7_UNORM_BLOCK),
         "BC7 is recognised as compressed");
   check(!pvr_bc_format_is_compressed(VK_FORMAT_R8G8B8A8_UNORM),
         "RGBA8 is not compressed");

   printf("\n[2] Decoded level sizing\n");
   check(pvr_bc_format_info(VK_FORMAT_BC1_RGB_UNORM_BLOCK, &info), "BC1 info");
   check(pvr_bc_decoded_level_size(&info, 64, 64) == 64 * 4 * 64,
         "64x64 BC1 decodes to 16384 bytes of RGBA8");
   check(pvr_bc_format_info(VK_FORMAT_BC6H_UFLOAT_BLOCK, &info), "BC6H info");
   check(pvr_bc_decoded_level_size(&info, 8, 8) == 8 * 8 * 8,
         "8x8 BC6H decodes to 512 bytes of RGBA16F");

   printf("\n[3] BC1 colour decode\n");
   {
      uint8_t block[8];
      uint8_t out[4 * 4 * 4];
      make_bc_color_block(block, true);
      check(pvr_bc_format_info(VK_FORMAT_BC1_RGB_UNORM_BLOCK, &info), "BC1 info");
      memset(out, 0xAB, sizeof(out));
      pvr_bc_decode_level(&info, out, 4 * 4, block, 8, 4, 4);
      check_pixel(out, 4 * 4, 0, 0, 255, 0, 0, 255, "BC1 (0,0) is pure red");
      check_pixel(out, 4 * 4, 3, 3, 255, 0, 0, 255, "BC1 (3,3) is pure red");
   }

   printf("\n[4] BC3 colour + alpha decode\n");
   {
      uint8_t block[16];
      uint8_t out[4 * 4 * 4];
      make_bc_color_block(block, false);
      check(pvr_bc_format_info(VK_FORMAT_BC3_UNORM_BLOCK, &info), "BC3 info");
      pvr_bc_decode_level(&info, out, 4 * 4, block, 16, 4, 4);
      check_pixel(out, 4 * 4, 2, 1, 255, 0, 0, 255, "BC3 (2,1) red, opaque");
   }

   printf("\n[5] BC4 single-channel decode\n");
   {
      /* Endpoints 255 and 0 with all indices 0 selects endpoint 0 = 255. */
      uint8_t block[8] = { 255, 0, 0, 0, 0, 0, 0, 0 };
      uint8_t out[4 * 4 * 4];
      check(pvr_bc_format_info(VK_FORMAT_BC4_UNORM_BLOCK, &info), "BC4 info");
      pvr_bc_decode_level(&info, out, 4 * 4, block, 8, 4, 4);
      check_pixel(out, 4 * 4, 1, 2, 255, 0, 0, 255,
                  "BC4 endpoint 0 expands to R=255, G=B=0, A=255");
   }

   printf("\n[6] BC7 and BC6H smoke decode\n");
   {
      uint8_t block[16] = { 0 };
      uint8_t out8[4 * 4 * 4];
      float out16[4 * 4 * 4];

      check(pvr_bc_format_info(VK_FORMAT_BC7_UNORM_BLOCK, &info), "BC7 info");
      pvr_bc_decode_level(&info, out8, 4 * 4, block, 16, 4, 4);
      check(true, "BC7 all-zero block decodes without crashing");

      check(pvr_bc_format_info(VK_FORMAT_BC6H_UFLOAT_BLOCK, &info), "BC6H info");
      pvr_bc_decode_level(&info, (uint8_t *)out16, 4 * 4 * 4, block, 16, 4, 4);
      check(out16[3] == 1.0f,
            "BC6H decode writes an opaque alpha (1.0)");
   }

   printf("\n[7] Partial (non-4-aligned) levels\n");
   {
      /* A 2x2 mip of a BC1 image still decodes from a whole 4x4 block. */
      uint8_t block[8];
      uint8_t out[2 * 2 * 4];
      make_bc_color_block(block, true);
      pvr_bc_format_info(VK_FORMAT_BC1_RGB_UNORM_BLOCK, &info);
      pvr_bc_decode_level(&info, out, 2 * 4, block, 8, 2, 2);
      check_pixel(out, 2 * 4, 1, 1, 255, 0, 0, 255,
                  "2x2 level decodes from a full block");
   }

   printf("\n== %d passed, %d failed ==\n", pass_count, fail_count);
   if (fail_count) {
      printf(">>> BC EMULATION DECODE: FAILURES <<<\n");
      return 1;
   }
   printf(">>> BC EMULATION DECODE: ALL CHECKS PASSED <<<\n");
   return 0;
}
