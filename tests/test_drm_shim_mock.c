/*
 * test_drm_shim_mock.c
 *
 * Host-only mock of the DRM-shim render node lifecycle used for M11 validation.
 * This mimics the offline open/connect/create-buffer/map/submit/close flow that
 * Mesa's DRM-shim stack expects, without touching the real device or requiring a
 * real /dev/dri/renderD128 node.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOCK_RENDER_NODE_PATH "/dev/dri/renderD128"
#define MOCK_DEVICE_VENDOR_ID 0x1010U
#define MOCK_DEVICE_DEVICE_ID 0x35010101U
#define MOCK_MAX_SUBMITS 8U
#define MOCK_MAX_EVENTS 16U

enum mock_event_kind {
   EVENT_OPEN = 0,
   EVENT_CONNECT,
   EVENT_BO_CREATE,
   EVENT_BO_MAP,
   EVENT_SUBMIT,
   EVENT_CLOSE,
   EVENT_FAILURE,
};

struct mock_event {
   enum mock_event_kind kind;
   uint32_t value;
};

struct mock_drm_shim {
   char path[64];
   bool opened;
   bool connected;
   bool bo_created;
   bool bo_mapped;
   bool active;
   uint32_t vendor_id;
   uint32_t device_id;
   uint32_t queue_id;
   uint32_t bo_handle;
   uint32_t map_offset;
   uint32_t submit_count;
   uint32_t next_event_index;
   struct mock_event events[MOCK_MAX_EVENTS];
};

static void mock_shim_log(struct mock_drm_shim *shim, enum mock_event_kind kind,
                          uint32_t value)
{
   if (!shim || shim->next_event_index >= MOCK_MAX_EVENTS) {
      return;
   }

   shim->events[shim->next_event_index].kind = kind;
   shim->events[shim->next_event_index].value = value;
   shim->next_event_index++;
}

static void mock_shim_reset(struct mock_drm_shim *shim)
{
   memset(shim, 0, sizeof(*shim));
   snprintf(shim->path, sizeof(shim->path), "%s", MOCK_RENDER_NODE_PATH);
   shim->vendor_id = MOCK_DEVICE_VENDOR_ID;
   shim->device_id = MOCK_DEVICE_DEVICE_ID;
   shim->queue_id = 0x42U;
   shim->bo_handle = 0x1000U;
   shim->map_offset = 0x2000U;
}

static int mock_shim_open(struct mock_drm_shim *shim)
{
   if (!shim) {
      return -1;
   }

   mock_shim_reset(shim);
   shim->opened = true;
   shim->active = true;
   mock_shim_log(shim, EVENT_OPEN, 0U);
   return 0;
}

static int mock_shim_connect(struct mock_drm_shim *shim)
{
   if (!shim || !shim->opened || !shim->active) {
      return -1;
   }

   shim->connected = true;
   mock_shim_log(shim, EVENT_CONNECT, shim->queue_id);
   return 0;
}

static int mock_shim_create_bo(struct mock_drm_shim *shim, uint32_t size_bytes)
{
   if (!shim || !shim->opened || !shim->connected || !shim->active) {
      return -1;
   }

   if (size_bytes == 0U || size_bytes > 16U * 1024U) {
      return -2;
   }

   shim->bo_created = true;
   shim->bo_handle = 0x2000U + size_bytes;
   mock_shim_log(shim, EVENT_BO_CREATE, size_bytes);
   return 0;
}

static int mock_shim_map_bo(struct mock_drm_shim *shim)
{
   if (!shim || !shim->opened || !shim->connected || !shim->bo_created || !shim->active) {
      return -1;
   }

   shim->bo_mapped = true;
   mock_shim_log(shim, EVENT_BO_MAP, shim->map_offset);
   return 0;
}

static int mock_shim_submit(struct mock_drm_shim *shim, uint32_t word_count)
{
   if (!shim || !shim->opened || !shim->connected || !shim->bo_mapped || !shim->active) {
      return -1;
   }

   if (word_count == 0U || word_count > 4096U) {
      return -2;
   }

   if (shim->submit_count >= MOCK_MAX_SUBMITS) {
      return -3;
   }

   shim->submit_count++;
   mock_shim_log(shim, EVENT_SUBMIT, word_count);
   return 0;
}

static int mock_shim_close(struct mock_drm_shim *shim)
{
   if (!shim || !shim->opened || !shim->active) {
      return -1;
   }

   shim->opened = false;
   shim->connected = false;
   shim->bo_created = false;
   shim->bo_mapped = false;
   shim->active = false;
   shim->submit_count = 0U;
   mock_shim_log(shim, EVENT_CLOSE, 0U);
   return 0;
}

static int test_drm_shim_sequence(void)
{
   struct mock_drm_shim shim;
   int status;

   puts("[M11 mock] Starting realistic render-node sequence...");

   status = mock_shim_open(&shim);
   if (status != 0) {
      fprintf(stderr, "mock_shim_open() failed\n");
      return 1;
   }

   if (strcmp(shim.path, MOCK_RENDER_NODE_PATH) != 0) {
      fprintf(stderr, "Unexpected render node path: %s\n", shim.path);
      return 1;
   }

   if (shim.vendor_id != MOCK_DEVICE_VENDOR_ID || shim.device_id != MOCK_DEVICE_DEVICE_ID) {
      fprintf(stderr, "Unexpected device identity\n");
      return 1;
   }

   status = mock_shim_connect(&shim);
   if (status != 0) {
      fprintf(stderr, "mock_shim_connect() failed\n");
      return 1;
   }

   status = mock_shim_create_bo(&shim, 4096U);
   if (status != 0) {
      fprintf(stderr, "mock_shim_create_bo() failed\n");
      return 1;
   }

   status = mock_shim_map_bo(&shim);
   if (status != 0) {
      fprintf(stderr, "mock_shim_map_bo() failed\n");
      return 1;
   }

   for (uint32_t i = 1U; i <= 4U; i++) {
      status = mock_shim_submit(&shim, 64U * i);
      if (status != 0) {
         fprintf(stderr, "mock_shim_submit() failed for submit %u\n", i);
         return 1;
      }
   }

   if (shim.submit_count != 4U) {
      fprintf(stderr, "Unexpected submit count: %u\n", shim.submit_count);
      return 1;
   }

   status = mock_shim_close(&shim);
   if (status != 0) {
      fprintf(stderr, "mock_shim_close() failed\n");
      return 1;
   }

   if (shim.next_event_index != 9U) {
      fprintf(stderr, "Unexpected event count: %u\n", shim.next_event_index);
      return 1;
   }

   puts("[M11 mock] Render node lifecycle complete: OPEN -> CONNECT -> BO_CREATE -> BO_MAP -> SUBMIT x4 -> CLOSE OK");
   return 0;
}

static int test_drm_shim_failure_cleanup(void)
{
   struct mock_drm_shim shim;
   int status;

   puts("[M11 mock] Checking failure cleanup path...");

   status = mock_shim_open(&shim);
   if (status != 0) {
      fprintf(stderr, "mock_shim_open() failed for failure cleanup test\n");
      return 1;
   }

   status = mock_shim_connect(&shim);
   if (status != 0) {
      fprintf(stderr, "mock_shim_connect() failed for failure cleanup test\n");
      return 1;
   }

   status = mock_shim_create_bo(&shim, 4096U);
   if (status != 0) {
      fprintf(stderr, "mock_shim_create_bo() failed for failure cleanup test\n");
      return 1;
   }

   status = mock_shim_map_bo(&shim);
   if (status != 0) {
      fprintf(stderr, "mock_shim_map_bo() failed for failure cleanup test\n");
      return 1;
   }

   status = mock_shim_submit(&shim, 0U);
   if (status == 0) {
      fprintf(stderr, "mock_shim_submit() incorrectly accepted invalid zero-sized work\n");
      return 1;
   }

   if (!shim.active || !shim.bo_mapped) {
      fprintf(stderr, "failure path left the shim in an invalid state\n");
      return 1;
   }

   status = mock_shim_submit(&shim, 64U);
   if (status != 0) {
      fprintf(stderr, "mock_shim_submit() rejected valid work after failure injection\n");
      return 1;
   }

   status = mock_shim_close(&shim);
   if (status != 0) {
      fprintf(stderr, "mock_shim_close() failed after failure cleanup\n");
      return 1;
   }

   puts("[M11 mock] Failure cleanup path complete: invalid submit rejected without corrupting node state.");
   return 0;
}

int main(void)
{
   puts("=================================================================");
   puts("  M11 Host-Only DRM/Shim Mock Pipeline Validation");
   puts("=================================================================");

   if (test_drm_shim_sequence() != 0) {
      puts("[M11 mock] FAIL: sequence validation failed.");
      return 1;
   }

   if (test_drm_shim_failure_cleanup() != 0) {
      puts("[M11 mock] FAIL: failure cleanup validation failed.");
      return 1;
   }

   puts("[M11 mock] PASS: host-side DRM-shim render-node lifecycle, queue submission, and failure cleanup checks passed.");
   return 0;
}
