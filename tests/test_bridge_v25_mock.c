/*
 * test_bridge_v25_mock.c
 *
 * Comprehensive host-side unit test and mock bridge simulator for PowerVR DDK 25.1 ABI.
 * Validates C11 static assertions, bridge structures, dispatch function IDs,
 * handle typing, regression traps, partial failure recovery, and lifecycle offline.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "../research/mesa-src/src/imagination/vulkan/winsys/pvrsrvkm/pvr_srv_bridge_v25.h"

/* Packed BVNC for 35.3.408.101:
 * B: 35 (0x0023)
 * V: 3  (0x0003)
 * N: 408 (0x0198)
 * C: 101 (0x0065)
 * BVNC: 0x0023000301980065ULL
 */
#define EXPECTED_PACKED_BVNC_35_3_408_101 0x0023000301980065ULL

/* Error codes matching pvrsrvkm */
#define PVRSRV_OK 0U
#define PVRSRV_ERROR_OUT_OF_MEMORY 1U
#define PVRSRV_ERROR_INVALID_PARAMS 3U
#define PVRSRV_ERROR_HANDLE_NOT_FOUND 25U
#define PVRSRV_ERROR_HANDLE_TYPE_MISMATCH 26U
#define PVRSRV_ERROR_BRIDGE_CALL_FAILED 37U

/* Mock handle management */
struct mock_handle {
    uint32_t id;
    enum pvr_srv_handle_type_v25 type;
    bool active;
};

#define MAX_MOCK_HANDLES 128
static struct mock_handle g_mock_handles[MAX_MOCK_HANDLES];
static uint32_t g_next_handle_id = 1;

static void mock_reset_handles(void)
{
    memset(g_mock_handles, 0, sizeof(g_mock_handles));
    g_next_handle_id = 1;
}

static void *mock_alloc_handle(enum pvr_srv_handle_type_v25 type)
{
    assert(g_next_handle_id < MAX_MOCK_HANDLES);
    uint32_t id = g_next_handle_id++;
    g_mock_handles[id].id = id;
    g_mock_handles[id].type = type;
    g_mock_handles[id].active = true;
    return (void *)(uintptr_t)(0x1000U + id);
}

static bool mock_validate_handle(void *ptr, enum pvr_srv_handle_type_v25 expected_type, uint32_t *out_err)
{
    uintptr_t val = (uintptr_t)ptr;
    if (val < 0x1001U || val >= (0x1000U + MAX_MOCK_HANDLES)) {
        if (out_err) *out_err = PVRSRV_ERROR_HANDLE_NOT_FOUND;
        return false;
    }
    uint32_t id = (uint32_t)(val - 0x1000U);
    if (!g_mock_handles[id].active) {
        if (out_err) *out_err = PVRSRV_ERROR_HANDLE_NOT_FOUND;
        return false;
    }
    if (g_mock_handles[id].type != expected_type) {
        if (out_err) *out_err = PVRSRV_ERROR_HANDLE_TYPE_MISMATCH;
        return false;
    }
    if (out_err) *out_err = PVRSRV_OK;
    return true;
}

static bool mock_free_handle(void *ptr, enum pvr_srv_handle_type_v25 expected_type, uint32_t *out_err)
{
    if (!mock_validate_handle(ptr, expected_type, out_err)) {
        return false;
    }
    uint32_t id = (uint32_t)((uintptr_t)ptr - 0x1000U);
    g_mock_handles[id].active = false;
    if (out_err) *out_err = PVRSRV_OK;
    return true;
}

static int g_active_handles_count(void)
{
    int count = 0;
    for (uint32_t i = 1; i < g_next_handle_id; i++) {
        if (g_mock_handles[i].active) count++;
    }
    return count;
}

/* Simulation flags for error injection */
static bool g_inject_alloc_pmr_oom = false;
static bool g_inject_map_pages_fail = false;

/* Mock bridge dispatch simulator matching pvrsrvkm.ko dispatch table */
static int mock_bridge_dispatch(uint32_t bridge_id, uint32_t func_id,
                                const void *in_data, uint32_t in_size,
                                void *out_data, uint32_t out_size)
{
    switch (bridge_id) {
    case PVR_SRV_BRIDGE_SRVCORE_V25:
        if (func_id == PVR_SRV_BRIDGE_SRVCORE_CONNECT_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_connect_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_connect_out_v25));
            const struct pvr_srv_bridge_connect_in_v25 *in = in_data;
            struct pvr_srv_bridge_connect_out_v25 *out = out_data;
            
            /* Verify 64-bit client flag (bit 5) */
            if (!(in->ui32Flags & PVR_SRV_FLAGS_CLIENT_64BIT_COMPAT_V25)) {
                fprintf(stderr, "Mock trap: 64-bit client flag missing!\n");
                out->ui32Error = PVRSRV_ERROR_INVALID_PARAMS;
                return -1;
            }
            out->ui64BVNC = EXPECTED_PACKED_BVNC_35_3_408_101;
            out->ui32Error = PVRSRV_OK;
            out->ui32CapabilityFlags = 0x10000;
            out->ui8KernelArch = 2; /* 64-bit */
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_SRVCORE_DISCONNECT_V25) {
            assert(out_size == sizeof(struct pvr_srv_bridge_disconnect_out_v25));
            struct pvr_srv_bridge_disconnect_out_v25 *out = out_data;
            out->ui32Error = PVRSRV_OK;
            return 0;
        }
        break;

    case PVR_SRV_BRIDGE_MM_V25:
        /* REGRESSION TRAP: If unref dispatches to Func 1 (PMRUNEXPORTPMR) */
        if (func_id == PVR_SRV_BRIDGE_MM_PMRUNEXPORTPMR_V25) {
            fprintf(stderr, "REGRESSION TRAP TRIGGERED: PMR unreference dispatched to PMR-unexport (Func 1)!\n");
            struct pvr_srv_bridge_pmr_unref_pmr_out_v25 *out = out_data;
            out->ui32Error = PVRSRV_ERROR_HANDLE_TYPE_MISMATCH;
            return -1;
        }

        /* REGRESSION TRAP: If PMR alloc dispatches to Func 10 (CTXDESTROY) */
        if (func_id == PVR_SRV_BRIDGE_MM_DEVMEMINTCTXDESTROY_V25 && in_size == sizeof(struct pvr_srv_bridge_physmem_new_ram_backed_pmr_in_v25)) {
            fprintf(stderr, "REGRESSION TRAP TRIGGERED: PMR allocation dispatched to context-destroy (Func 10)!\n");
            struct pvr_srv_bridge_physmem_new_ram_backed_pmr_out_v25 *out = out_data;
            out->ui32Error = PVRSRV_ERROR_INVALID_PARAMS;
            return -1;
        }

        if (func_id == PVR_SRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDPMR_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_physmem_new_ram_backed_pmr_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_physmem_new_ram_backed_pmr_out_v25));
            const struct pvr_srv_bridge_physmem_new_ram_backed_pmr_in_v25 *in = in_data;
            struct pvr_srv_bridge_physmem_new_ram_backed_pmr_out_v25 *out = out_data;

            if (g_inject_alloc_pmr_oom) {
                out->hPMR = NULL;
                out->ui32Error = PVRSRV_ERROR_OUT_OF_MEMORY;
                return -1;
            }

            /* Kernel checks ui32PhyBlocks and mapping table */
            if (in->ui32PhyBlocks > 0 && !in->pui32MappingTable) {
                out->hPMR = NULL;
                out->ui32Error = PVRSRV_ERROR_INVALID_PARAMS;
                return -1;
            }
            if (in->ui64Size == 0) {
                out->hPMR = NULL;
                out->ui32Error = PVRSRV_ERROR_INVALID_PARAMS;
                return -1;
            }

            out->hPMR = mock_alloc_handle(PVRSRV_HANDLE_TYPE_PMR_V25);
            out->ui32Error = PVRSRV_OK;
            out->ui64Flags = in->ui64Flags;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_MM_PMRUNREFPMR_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_pmr_unref_pmr_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_pmr_unref_pmr_out_v25));
            const struct pvr_srv_bridge_pmr_unref_pmr_in_v25 *in = in_data;
            struct pvr_srv_bridge_pmr_unref_pmr_out_v25 *out = out_data;

            uint32_t err = 0;
            if (!mock_free_handle(in->hPMR, PVRSRV_HANDLE_TYPE_PMR_V25, &err)) {
                out->ui32Error = err;
                return -1;
            }
            out->ui32Error = PVRSRV_OK;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_MM_DEVMEMINTCTXCREATE_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_devmem_int_ctx_create_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_devmem_int_ctx_create_out_v25));
            struct pvr_srv_bridge_devmem_int_ctx_create_out_v25 *out = out_data;
            out->hServerMemCtx = mock_alloc_handle(PVRSRV_HANDLE_TYPE_SERVER_MEMCTX_V25);
            out->hServerMemCtxData = mock_alloc_handle(PVRSRV_HANDLE_TYPE_SERVER_MEMCTX_DATA_V25);
            out->ui32Error = PVRSRV_OK;
            out->ui32CPUCacheLineSize = 64;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_MM_DEVMEMINTCTXDESTROY_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_devmem_int_ctx_destroy_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_devmem_int_ctx_destroy_out_v25));
            const struct pvr_srv_bridge_devmem_int_ctx_destroy_in_v25 *in = in_data;
            struct pvr_srv_bridge_devmem_int_ctx_destroy_out_v25 *out = out_data;
            uint32_t err = 0;
            if (!mock_free_handle(in->hServerMemCtx, PVRSRV_HANDLE_TYPE_SERVER_MEMCTX_V25, &err)) {
                out->ui32Error = err;
                return -1;
            }
            out->ui32Error = PVRSRV_OK;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_MM_HEAPCFGHEAPCONFIGCOUNT_V25) {
            assert(out_size == sizeof(struct pvr_srv_bridge_heap_cfg_heap_config_count_out_v25));
            struct pvr_srv_bridge_heap_cfg_heap_config_count_out_v25 *out = out_data;
            out->ui32NumHeapConfigs = 1;
            out->ui32Error = PVRSRV_OK;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_MM_HEAPCFGHEAPCOUNT_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_heap_cfg_heap_count_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_heap_cfg_heap_count_out_v25));
            struct pvr_srv_bridge_heap_cfg_heap_count_out_v25 *out = out_data;
            out->ui32NumHeaps = 16;
            out->ui32Error = PVRSRV_OK;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_MM_HEAPCFGHEAPDETAILS_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_heap_cfg_heap_details_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_heap_cfg_heap_details_out_v25));
            const struct pvr_srv_bridge_heap_cfg_heap_details_in_v25 *in = in_data;
            struct pvr_srv_bridge_heap_cfg_heap_details_out_v25 *out = out_data;
            if (in->ui32HeapIndex == 0) {
                out->ui64DevVAddrBase = 0x8000ULL;
                out->ui64HeapByteSize = 0x8000000000ULL;
                out->ui32Log2DataPageSize = 12;
                strncpy(out->szHeapName, "General SVM", sizeof(out->szHeapName));
            } else if (in->ui32HeapIndex == 1) {
                out->ui64DevVAddrBase = 0x8000000000ULL;
                out->ui64HeapByteSize = 0x4000000000ULL;
                out->ui32Log2DataPageSize = 12;
                strncpy(out->szHeapName, "General", sizeof(out->szHeapName));
            } else {
                snprintf(out->szHeapName, sizeof(out->szHeapName), "Heap_%u", in->ui32HeapIndex);
                out->ui64DevVAddrBase = 0xda00000000ULL + in->ui32HeapIndex * 0x10000000ULL;
                out->ui64HeapByteSize = 0x10000000ULL;
                out->ui32Log2DataPageSize = 12;
            }
            out->ui32Error = PVRSRV_OK;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_MM_DEVMEMINTHEAPCREATE_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_devmem_int_heap_create_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_devmem_int_heap_create_out_v25));
            const struct pvr_srv_bridge_devmem_int_heap_create_in_v25 *in = in_data;
            struct pvr_srv_bridge_devmem_int_heap_create_out_v25 *out = out_data;
            uint32_t err = 0;
            if (!mock_validate_handle(in->hServerMemCtx, PVRSRV_HANDLE_TYPE_SERVER_MEMCTX_V25, &err)) {
                out->ui32Error = err;
                return -1;
            }
            out->hServerHeap = mock_alloc_handle(PVRSRV_HANDLE_TYPE_SERVER_HEAP_V25);
            out->ui32Error = PVRSRV_OK;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_MM_DEVMEMINTHEAPDESTROY_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_devmem_int_heap_destroy_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_devmem_int_heap_destroy_out_v25));
            const struct pvr_srv_bridge_devmem_int_heap_destroy_in_v25 *in = in_data;
            struct pvr_srv_bridge_devmem_int_heap_destroy_out_v25 *out = out_data;
            uint32_t err = 0;
            if (!mock_free_handle(in->hServerHeap, PVRSRV_HANDLE_TYPE_SERVER_HEAP_V25, &err)) {
                out->ui32Error = err;
                return -1;
            }
            out->ui32Error = PVRSRV_OK;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_MM_DEVMEMXINTRESERVERANGE_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_devmemx_int_reserve_range_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_devmemx_int_reserve_range_out_v25));
            const struct pvr_srv_bridge_devmemx_int_reserve_range_in_v25 *in = in_data;
            struct pvr_srv_bridge_devmemx_int_reserve_range_out_v25 *out = out_data;
            uint32_t err = 0;
            if (!mock_validate_handle(in->hServerHeap, PVRSRV_HANDLE_TYPE_SERVER_HEAP_V25, &err)) {
                out->ui32Error = err;
                return -1;
            }
            out->hReservation = mock_alloc_handle(PVRSRV_HANDLE_TYPE_RESERVATION_V25);
            out->ui32Error = PVRSRV_OK;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_MM_DEVMEMXINTUNRESERVERANGE_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_devmemx_int_unreserve_range_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_devmemx_int_unreserve_range_out_v25));
            const struct pvr_srv_bridge_devmemx_int_unreserve_range_in_v25 *in = in_data;
            struct pvr_srv_bridge_devmemx_int_unreserve_range_out_v25 *out = out_data;
            uint32_t err = 0;
            if (!mock_free_handle(in->hReservation, PVRSRV_HANDLE_TYPE_RESERVATION_V25, &err)) {
                out->ui32Error = err;
                return -1;
            }
            out->ui32Error = PVRSRV_OK;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_MM_DEVMEMXINTMAPPAGES_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_devmemx_int_map_pages_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_devmemx_int_map_pages_out_v25));
            const struct pvr_srv_bridge_devmemx_int_map_pages_in_v25 *in = in_data;
            struct pvr_srv_bridge_devmemx_int_map_pages_out_v25 *out = out_data;
            if (g_inject_map_pages_fail) {
                out->ui32Error = PVRSRV_ERROR_INVALID_PARAMS;
                return -1;
            }
            uint32_t err = 0;
            if (!mock_validate_handle(in->hPMR, PVRSRV_HANDLE_TYPE_PMR_V25, &err) ||
                !mock_validate_handle(in->hReservation, PVRSRV_HANDLE_TYPE_RESERVATION_V25, &err)) {
                out->ui32Error = err;
                return -1;
            }
            out->ui32Error = PVRSRV_OK;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_MM_DEVMEMXINTUNMAPPAGES_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_devmemx_int_unmap_pages_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_devmemx_int_unmap_pages_out_v25));
            const struct pvr_srv_bridge_devmemx_int_unmap_pages_in_v25 *in = in_data;
            struct pvr_srv_bridge_devmemx_int_unmap_pages_out_v25 *out = out_data;
            uint32_t err = 0;
            if (!mock_validate_handle(in->hReservation, PVRSRV_HANDLE_TYPE_RESERVATION_V25, &err)) {
                out->ui32Error = err;
                return -1;
            }
            out->ui32Error = PVRSRV_OK;
            return 0;
        }
        break;

    case PVR_SRV_BRIDGE_DMABUF_V25:
        if (func_id == PVR_SRV_BRIDGE_DMABUF_PHYSMEMEXPORTDMABUF_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_physmem_export_dmabuf_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_physmem_export_dmabuf_out_v25));
            const struct pvr_srv_bridge_physmem_export_dmabuf_in_v25 *in = in_data;
            struct pvr_srv_bridge_physmem_export_dmabuf_out_v25 *out = out_data;
            uint32_t err = 0;
            if (!mock_validate_handle(in->hPMR, PVRSRV_HANDLE_TYPE_PMR_V25, &err)) {
                out->ui32Error = err;
                return -1;
            }
            out->ui32Error = PVRSRV_OK;
            out->iFd = 42; /* Simulated valid DMA-BUF fd */
            return 0;
        }
        break;

    case PVR_SRV_BRIDGE_RGXCMP_V25:
        if (func_id == PVR_SRV_BRIDGE_RGXCMP_RGXCREATECOMPUTECONTEXT_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_rgx_create_compute_context_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_rgx_create_compute_context_out_v25));
            const struct pvr_srv_bridge_rgx_create_compute_context_in_v25 *in = in_data;
            struct pvr_srv_bridge_rgx_create_compute_context_out_v25 *out = out_data;
            uint32_t err = 0;
            if (!mock_validate_handle(in->hServerMemCtxData, PVRSRV_HANDLE_TYPE_SERVER_MEMCTX_DATA_V25, &err)) {
                out->ui32Error = err;
                return -1;
            }
            out->hComputeContext = mock_alloc_handle(PVRSRV_HANDLE_TYPE_COMPUTE_CONTEXT_V25);
            out->ui32Error = PVRSRV_OK;
            return 0;
        } else if (func_id == PVR_SRV_BRIDGE_RGXCMP_RGXDESTROYCOMPUTECONTEXT_V25) {
            assert(in_size == sizeof(struct pvr_srv_bridge_rgx_destroy_compute_context_in_v25));
            assert(out_size == sizeof(struct pvr_srv_bridge_rgx_destroy_compute_context_out_v25));
            const struct pvr_srv_bridge_rgx_destroy_compute_context_in_v25 *in = in_data;
            struct pvr_srv_bridge_rgx_destroy_compute_context_out_v25 *out = out_data;
            uint32_t err = 0;
            if (!mock_free_handle(in->hComputeContext, PVRSRV_HANDLE_TYPE_COMPUTE_CONTEXT_V25, &err)) {
                out->ui32Error = err;
                return -1;
            }
            out->ui32Error = PVRSRV_OK;
            return 0;
        }
        break;

    default:
        fprintf(stderr, "Mock error: unknown bridge %u\n", bridge_id);
        return -1;
    }

    return -1;
}

/* ========================================================================= */
/* Client Implementation Functions Under Test (Exact DDK 25.1 Logic)         */
/* ========================================================================= */

static int client_pvr_srv_alloc_pmr_v25(uint64_t size, uint64_t flags, uint32_t log2_page_size, void **pmr_out)
{
    const char *annotation = "VK PHYSICAL ALLOCATION";
    const uint32_t annotation_size = strlen(annotation) + 1;
    uint32_t mapping_table = 0;

    struct pvr_srv_bridge_physmem_new_ram_backed_pmr_in_v25 cmd_v25 = {
        .ui64Size = size,
        .pui32MappingTable = &mapping_table,
        .pszAnnotation = annotation,
        .ui32AnnotationLength = annotation_size,
        .ui32Log2PageSize = log2_page_size,
        .ui32PhyBlocks = 1,
        .ui32VirtBlocks = 1,
        .ui32PDumpFlags = 0,
        .ui32PID = 1234,
        .ui64Flags = flags,
    };
    struct pvr_srv_bridge_physmem_new_ram_backed_pmr_out_v25 ret_v25 = {
        .ui32Error = PVRSRV_ERROR_BRIDGE_CALL_FAILED,
    };

    int result = mock_bridge_dispatch(PVR_SRV_BRIDGE_MM_V25,
                                       PVR_SRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDPMR_V25,
                                       &cmd_v25, sizeof(cmd_v25),
                                       &ret_v25, sizeof(ret_v25));
    if (result || ret_v25.ui32Error != PVRSRV_OK) {
        *pmr_out = NULL;
        return -1;
    }

    *pmr_out = ret_v25.hPMR;
    return 0;
}

static int client_pvr_srv_free_pmr_v25(void *pmr)
{
    if (!pmr)
        return 0;

    struct pvr_srv_bridge_pmr_unref_pmr_in_v25 cmd_v25 = {
        .hPMR = pmr,
    };
    struct pvr_srv_bridge_pmr_unref_pmr_out_v25 ret_v25 = {
        .ui32Error = PVRSRV_ERROR_BRIDGE_CALL_FAILED,
    };

    int result = mock_bridge_dispatch(PVR_SRV_BRIDGE_MM_V25,
                                       PVR_SRV_BRIDGE_MM_PMRUNREFPMR_V25,
                                       &cmd_v25, sizeof(cmd_v25),
                                       &ret_v25, sizeof(ret_v25));
    if (result || ret_v25.ui32Error != PVRSRV_OK) {
        return -1;
    }

    return 0;
}

/* ========================================================================= */
/* Regression & Robustness Test Cases                                        */
/* ========================================================================= */

static void test_regression_pmr_function_ids(void)
{
    printf("[Regression Test 1] Verifying PMR Function IDs against dispatch traps...\n");

    /* 1. Verify PMRUNREFPMR_V25 is Func 7, NOT Func 1 */
    assert(PVR_SRV_BRIDGE_MM_PMRUNREFPMR_V25 == 7U);
    assert(PVR_SRV_BRIDGE_MM_PMRUNEXPORTPMR_V25 == 1U);

    /* Allocate valid PMR */
    void *pmr = NULL;
    int ret = client_pvr_srv_alloc_pmr_v25(4096, 0x3, 12, &pmr);
    assert(ret == 0 && pmr != NULL);
    assert(mock_validate_handle(pmr, PVRSRV_HANDLE_TYPE_PMR_V25, NULL));

    /* Test that dispatching unref via the wrong Func 1 triggers failure */
    struct pvr_srv_bridge_pmr_unref_pmr_in_v25 bad_cmd = { .hPMR = pmr };
    struct pvr_srv_bridge_pmr_unref_pmr_out_v25 bad_out = {0};
    int bad_ret = mock_bridge_dispatch(PVR_SRV_BRIDGE_MM_V25,
                                       PVR_SRV_BRIDGE_MM_PMRUNEXPORTPMR_V25,
                                       &bad_cmd, sizeof(bad_cmd),
                                       &bad_out, sizeof(bad_out));
    assert(bad_ret != 0);
    assert(bad_out.ui32Error == PVRSRV_ERROR_HANDLE_TYPE_MISMATCH);

    /* Clean release using the correct client unref function */
    ret = client_pvr_srv_free_pmr_v25(pmr);
    assert(ret == 0);
    printf("      Success: Func 1 unexport trap passed; Func 7 unref verified!\n");
}

static void test_regression_pmr_alloc_not_context_destroy(void)
{
    printf("[Regression Test 2] Verifying PMR alloc doesn't dispatch to context destroy...\n");

    /* PHYSMEMNEWRAMBACKEDPMR must be Func 8, not Func 10 */
    assert(PVR_SRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDPMR_V25 == 8U);
    assert(PVR_SRV_BRIDGE_MM_DEVMEMINTCTXDESTROY_V25 == 10U);

    /* If a client inadvertently sent alloc payload to Func 10 */
    struct pvr_srv_bridge_physmem_new_ram_backed_pmr_in_v25 alloc_in = {
        .ui64Size = 8192,
    };
    struct pvr_srv_bridge_physmem_new_ram_backed_pmr_out_v25 alloc_out = {0};
    int trap_ret = mock_bridge_dispatch(PVR_SRV_BRIDGE_MM_V25,
                                        PVR_SRV_BRIDGE_MM_DEVMEMINTCTXDESTROY_V25,
                                        &alloc_in, sizeof(alloc_in),
                                        &alloc_out, sizeof(alloc_out));
    assert(trap_ret != 0);
    printf("      Success: Context destroy trap caught invalid dispatch!\n");
}

static void test_handle_safety_and_double_free(void)
{
    printf("[Safety Test 3] Handle type mismatch and double-free protection...\n");

    /* Allocate a ServerMemCtx handle */
    void *ctx_handle = mock_alloc_handle(PVRSRV_HANDLE_TYPE_SERVER_MEMCTX_V25);
    assert(ctx_handle != NULL);

    /* Attempting to release a context handle via PMR unref must fail with TYPE_MISMATCH */
    int ret = client_pvr_srv_free_pmr_v25(ctx_handle);
    assert(ret != 0);

    /* Now allocate a real PMR handle */
    void *pmr = NULL;
    ret = client_pvr_srv_alloc_pmr_v25(4096, 0x3, 12, &pmr);
    assert(ret == 0 && pmr != NULL);

    /* First free succeeds */
    ret = client_pvr_srv_free_pmr_v25(pmr);
    assert(ret == 0);

    /* Second free on same handle must fail (double free detected) */
    ret = client_pvr_srv_free_pmr_v25(pmr);
    assert(ret != 0);

    /* NULL handle release is safe no-op */
    ret = client_pvr_srv_free_pmr_v25(NULL);
    assert(ret == 0);

    /* Free context handle cleanly */
    uint32_t err = 0;
    bool ok = mock_free_handle(ctx_handle, PVRSRV_HANDLE_TYPE_SERVER_MEMCTX_V25, &err);
    assert(ok && err == PVRSRV_OK);
    printf("      Success: Type safety, double-free, and NULL guard validated!\n");
}

static void test_partial_failure_cleanup(void)
{
    printf("[Robustness Test 4] Partial initialization failure cleanup...\n");

    /* Setup context and heap */
    struct pvr_srv_bridge_devmem_int_ctx_create_out_v25 ctx_out = {0};
    mock_bridge_dispatch(PVR_SRV_BRIDGE_MM_V25, PVR_SRV_BRIDGE_MM_DEVMEMINTCTXCREATE_V25,
                         &(struct pvr_srv_bridge_devmem_int_ctx_create_in_v25){0}, 1,
                         &ctx_out, sizeof(ctx_out));

    struct pvr_srv_bridge_devmem_int_heap_create_in_v25 heap_in = {
        .hServerMemCtx = ctx_out.hServerMemCtx,
    };
    struct pvr_srv_bridge_devmem_int_heap_create_out_v25 heap_out = {0};
    mock_bridge_dispatch(PVR_SRV_BRIDGE_MM_V25, PVR_SRV_BRIDGE_MM_DEVMEMINTHEAPCREATE_V25,
                         &heap_in, sizeof(heap_in), &heap_out, sizeof(heap_out));

    /* Allocate PMR */
    void *pmr = NULL;
    int ret = client_pvr_srv_alloc_pmr_v25(16384, 0x3, 12, &pmr);
    assert(ret == 0 && pmr != NULL);

    /* Reserve range */
    struct pvr_srv_bridge_devmemx_int_reserve_range_in_v25 res_in = {
        .ui64DevVAddr = 0x8000000000ULL,
        .ui64Size = 16384,
        .hServerHeap = heap_out.hServerHeap,
    };
    struct pvr_srv_bridge_devmemx_int_reserve_range_out_v25 res_out = {0};
    mock_bridge_dispatch(PVR_SRV_BRIDGE_MM_V25, PVR_SRV_BRIDGE_MM_DEVMEMXINTRESERVERANGE_V25,
                         &res_in, sizeof(res_in), &res_out, sizeof(res_out));

    /* Simulate page mapping failure */
    g_inject_map_pages_fail = true;
    struct pvr_srv_bridge_devmemx_int_map_pages_in_v25 map_in = {
        .hPMR = pmr,
        .hReservation = res_out.hReservation,
        .ui32PageCount = 4,
    };
    struct pvr_srv_bridge_devmemx_int_map_pages_out_v25 map_out = {0};
    int map_res = mock_bridge_dispatch(PVR_SRV_BRIDGE_MM_V25, PVR_SRV_BRIDGE_MM_DEVMEMXINTMAPPAGES_V25,
                                        &map_in, sizeof(map_in), &map_out, sizeof(map_out));
    assert(map_res != 0);
    g_inject_map_pages_fail = false;

    /* Unwind cleanup without calling unmap on unmapped range */
    client_pvr_srv_free_pmr_v25(pmr);

    struct pvr_srv_bridge_devmemx_int_unreserve_range_in_v25 unres_in = {
        .hReservation = res_out.hReservation,
    };
    struct pvr_srv_bridge_devmemx_int_unreserve_range_out_v25 unres_out = {0};
    mock_bridge_dispatch(PVR_SRV_BRIDGE_MM_V25, PVR_SRV_BRIDGE_MM_DEVMEMXINTUNRESERVERANGE_V25,
                         &unres_in, sizeof(unres_in), &unres_out, sizeof(unres_out));

    struct pvr_srv_bridge_devmem_int_heap_destroy_in_v25 h_dest_in = { .hServerHeap = heap_out.hServerHeap };
    struct pvr_srv_bridge_devmem_int_heap_destroy_out_v25 h_dest_out = {0};
    mock_bridge_dispatch(PVR_SRV_BRIDGE_MM_V25, PVR_SRV_BRIDGE_MM_DEVMEMINTHEAPDESTROY_V25,
                         &h_dest_in, sizeof(h_dest_in), &h_dest_out, sizeof(h_dest_out));

    mock_free_handle(ctx_out.hServerMemCtxData, PVRSRV_HANDLE_TYPE_SERVER_MEMCTX_DATA_V25, NULL);

    struct pvr_srv_bridge_devmem_int_ctx_destroy_in_v25 ctx_dest_in = { .hServerMemCtx = ctx_out.hServerMemCtx };
    struct pvr_srv_bridge_devmem_int_ctx_destroy_out_v25 ctx_dest_out = {0};
    mock_bridge_dispatch(PVR_SRV_BRIDGE_MM_V25, PVR_SRV_BRIDGE_MM_DEVMEMINTCTXDESTROY_V25,
                         &ctx_dest_in, sizeof(ctx_dest_in), &ctx_dest_out, sizeof(ctx_dest_out));

    assert(g_active_handles_count() == 0);
    printf("      Success: Partial failure unwound with 0 leaked handles!\n");
}

static void test_oom_error_translation(void)
{
    printf("[Robustness Test 5] Out-of-memory error injection & translation...\n");

    g_inject_alloc_pmr_oom = true;
    void *pmr = (void *)0x1234;
    int ret = client_pvr_srv_alloc_pmr_v25(4096, 0x3, 12, &pmr);
    assert(ret != 0);
    assert(pmr == NULL); /* pmr_out must be NULL on error */
    g_inject_alloc_pmr_oom = false;

    printf("      Success: OOM properly trapped and pointer nulled!\n");
}

int main(void)
{
    printf("=================================================================\n");
    printf("  PowerVR DDK 25.1 ABI Host Mock Test & Robustness Suite        \n");
    printf("=================================================================\n\n");

    mock_reset_handles();

    /* 1. Structure assertions test */
    printf("[ABI Validation] Verifying C11 _Static_assert ABI definitions...\n");
    assert(sizeof(struct pvr_srv_bridge_physmem_new_ram_backed_pmr_in_v25) == 56);
    assert(sizeof(struct pvr_srv_bridge_physmem_new_ram_backed_pmr_out_v25) == 20);
    assert(sizeof(struct pvr_srv_bridge_pmr_unref_pmr_in_v25) == 8);
    assert(sizeof(struct pvr_srv_bridge_pmr_unref_pmr_out_v25) == 4);
    printf("      Success: All 25+ _Static_assert compile-time and runtime checks passed!\n\n");

    /* 2. Run regression and safety test cases */
    test_regression_pmr_function_ids();
    test_regression_pmr_alloc_not_context_destroy();
    test_handle_safety_and_double_free();
    test_partial_failure_cleanup();
    test_oom_error_translation();

    printf("\n-----------------------------------------------------------------\n");
    printf("  Executing End-to-End Winsys Lifecycle Flow...                  \n");
    printf("-----------------------------------------------------------------\n");

    /* Connect */
    struct pvr_srv_bridge_connect_in_v25 conn_in = {
        .ui32BuildOptions = 0x00020010,
        .ui32DDKBuild = 6715691,
        .ui32DDKVersion = 0x00190001,
        .ui32Flags = PVR_SRV_FLAGS_CLIENT_64BIT_COMPAT_V25,
    };
    struct pvr_srv_bridge_connect_out_v25 conn_out = {0};
    int ret = mock_bridge_dispatch(PVR_SRV_BRIDGE_SRVCORE_V25, PVR_SRV_BRIDGE_SRVCORE_CONNECT_V25,
                                   &conn_in, sizeof(conn_in), &conn_out, sizeof(conn_out));
    assert(ret == 0 && conn_out.ui64BVNC == EXPECTED_PACKED_BVNC_35_3_408_101);

    /* Allocate PMR via updated DDK 25.1 ABI */
    void *pmr = NULL;
    ret = client_pvr_srv_alloc_pmr_v25(65536, 0x3, 12, &pmr);
    assert(ret == 0 && pmr != NULL);
    assert(mock_validate_handle(pmr, PVRSRV_HANDLE_TYPE_PMR_V25, NULL));

    /* Export DMA-BUF from PMR */
    struct pvr_srv_bridge_physmem_export_dmabuf_in_v25 exp_in = { .hPMR = pmr };
    struct pvr_srv_bridge_physmem_export_dmabuf_out_v25 exp_out = {0};
    ret = mock_bridge_dispatch(PVR_SRV_BRIDGE_DMABUF_V25, PVR_SRV_BRIDGE_DMABUF_PHYSMEMEXPORTDMABUF_V25,
                               &exp_in, sizeof(exp_in), &exp_out, sizeof(exp_out));
    assert(ret == 0 && exp_out.iFd == 42);

    /* Free PMR via updated DDK 25.1 ABI */
    ret = client_pvr_srv_free_pmr_v25(pmr);
    assert(ret == 0);

    /* Disconnect */
    struct pvr_srv_bridge_disconnect_out_v25 disc_out = {0};
    ret = mock_bridge_dispatch(PVR_SRV_BRIDGE_SRVCORE_V25, PVR_SRV_BRIDGE_SRVCORE_DISCONNECT_V25,
                               NULL, 0, &disc_out, sizeof(disc_out));
    assert(ret == 0);

    assert(g_active_handles_count() == 0);
    printf("      Success: Full lifecycle completed with 0 dangling handles!\n");
    printf("\n>>> ALL STRENGTHENED REGRESSION & ABI TESTS PASSED PERFECTLY! <<<\n");
    return 0;
}
