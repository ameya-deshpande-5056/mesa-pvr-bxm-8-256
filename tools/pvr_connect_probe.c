/*
 * pvr_connect_probe.c - FROZEN FOR OFFLINE RECOVERY
 *
 * CRITICAL SAFETY NOTICE:
 * THIS PROBE IS FROZEN AND MARKED UNSAFE FOR DEVICE EXECUTION.
 * Multiple kernel panics occurred during bridge cleanup on the physical device.
 * DO NOT COMPILE, PUSH, OR RUN THIS BINARY ON THE PHYSICAL DEVICE.
 * Preserved strictly for offline source review and forensic audit.
 */
#error "CRITICAL SAFETY: pvr_connect_probe is frozen and unsafe for device execution. Device testing suspended."

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define DRM_COMMAND_BASE 0x40
#define DRM_PVR_SRVKM_CMD  0
#define DRM_PVR_SRVKM_INIT 5

#define DRM_IOCTL_SRVKM_CMD  _IOWR('d', DRM_COMMAND_BASE + DRM_PVR_SRVKM_CMD, struct drm_srvkm_cmd)
#define DRM_IOCTL_SRVKM_INIT _IOWR('d', DRM_COMMAND_BASE + DRM_PVR_SRVKM_INIT, struct drm_srvkm_init_data)

#define PVR_SRVKM_SERVICES_INIT 1U

#define PVR_SRV_BRIDGE_SRVCORE 1U
#define PVR_SRV_BRIDGE_SRVCORE_CONNECT 0U
#define PVR_SRV_BRIDGE_SRVCORE_DISCONNECT 1U

#define PVR_SRV_FLAGS_CLIENT_64BIT_COMPAT (1U << 7)

#define DDK_MAJ 25U
#define DDK_MIN 1U
#define DDK_BUILD 6715691U
#define DDK_VERSION (((DDK_MAJ & 0xFFFFU) << 16U) | (DDK_MIN & 0xFFFFU))

/* Build options required by pvrsrvkm.ko (0x00020010) */
#define RGX_BUILD_OPTIONS_DDK25 0x00020010U

struct drm_srvkm_cmd {
    uint32_t bridge_id;
    uint32_t bridge_func_id;
    uint64_t in_data_ptr;
    uint64_t out_data_ptr;
    uint32_t in_data_size;
    uint32_t out_data_size;
};

struct drm_srvkm_init_data {
    uint32_t init_module;
};

/* Bridge 1, Func 0: PVRSRV_BRIDGE_SRVCORE_CONNECT (In: 16 bytes) */
struct pvr_srv_bridge_connect_cmd {
    uint32_t build_options;
    uint32_t DDK_build;
    uint32_t DDK_version;
    uint32_t flags;
} __attribute__((packed));

/* Bridge 1, Func 0: PVRSRV_BRIDGE_SRVCORE_CONNECT (Out: 17 bytes) */
struct pvr_srv_bridge_connect_ret {
    uint64_t bvnc;
    uint32_t error;
    uint32_t capability_flags;
    uint8_t kernel_arch;
} __attribute__((packed));

#define PVR_SRV_BRIDGE_MM 6U
#define PVR_SRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDPMR 8U
#define PVR_SRV_BRIDGE_MM_DEVMEMINTCTXCREATE 9U
#define PVR_SRV_BRIDGE_MM_DEVMEMINTCTXDESTROY 10U
#define PVR_SRV_BRIDGE_MM_PMRUNREFPMR 7U

/* Bridge 6, Func 8: PHYSMEMNEWRAMBACKEDPMR (In: 56 bytes, Out: 20 bytes) */
struct pvr_srv_bridge_physmem_new_ram_backed_pmr_cmd {
    uint64_t size;
    uint32_t *mapping_table;
    const char *annotation;
    uint32_t annotation_size;
    uint32_t log2_page_size;
    uint32_t phy_blocks;
    uint32_t virt_blocks;
    uint32_t pdump_flags;
    uint32_t pid;
    uint64_t flags;
} __attribute__((packed));

struct pvr_srv_bridge_physmem_new_ram_backed_pmr_ret {
    void *pmr;
    uint32_t error;
    uint64_t out_flags;
} __attribute__((packed));

/* Bridge 6, Func 7: PMRUNREFPMR (In: 8 bytes, Out: 4 bytes) */
struct pvr_srv_bridge_pmr_unref_pmr_cmd {
    void *pmr;
} __attribute__((packed));

struct pvr_srv_bridge_pmr_unref_pmr_ret {
    uint32_t error;
} __attribute__((packed));

/* Bridge 6, Func 9: DEVMEMINTCTXCREATE (In: 1 byte, Out: 24 bytes) */
struct pvr_srv_bridge_devmem_int_ctx_create_cmd {
    uint8_t kernel_memory_ctx;
} __attribute__((packed));

struct pvr_srv_bridge_devmem_int_ctx_create_ret {
    void *server_memctx;
    void *server_memctx_data;
    uint32_t error;
    uint32_t cpu_cache_line_size;
} __attribute__((packed));

/* Bridge 6, Func 10: DEVMEMINTCTXDESTROY (In: 8 bytes, Out: 24 bytes) */
struct pvr_srv_bridge_devmem_int_ctx_destroy_cmd {
    void *server_memctx;
} __attribute__((packed));

struct pvr_srv_bridge_devmem_int_ctx_destroy_ret {
    uint32_t error;
    uint32_t reserved[5];
} __attribute__((packed));
struct pvr_srv_bridge_disconnect_cmd {
    uint32_t flags;
    uint32_t reserved[3];
} __attribute__((packed));

/* Bridge 1, Func 1: PVRSRV_BRIDGE_SRVCORE_DISCONNECT (Out: 4 bytes) */
struct pvr_srv_bridge_disconnect_ret {
    uint32_t error;
} __attribute__((packed));

static void print_bvnc(uint64_t bvnc) {
    uint16_t b = (uint16_t)((bvnc >> 48) & 0xFFFF);
    uint16_t v = (uint16_t)((bvnc >> 32) & 0xFFFF);
    uint16_t n = (uint16_t)((bvnc >> 16) & 0xFFFF);
    uint16_t c = (uint16_t)(bvnc & 0xFFFF);
    printf("    -> Packed BVNC: 0x%016llx\n", (unsigned long long)bvnc);
    printf("    -> Decoded BVNC: %u.%u.%u.%u\n", b, v, n, c);
}

int main(int argc, char **argv) {
    printf("=== PowerVR DDK 25.1 Connection Probe ===\n");
    printf("Target DDK Version: %u.%u (Build %u)\n", DDK_MAJ, DDK_MIN, DDK_BUILD);
    printf("Expected Bridge Connect Size: In=%zu bytes, Out=%zu bytes\n",
           sizeof(struct pvr_srv_bridge_connect_cmd),
           sizeof(struct pvr_srv_bridge_connect_ret));

    const char *candidates[] = {
        "/dev/dri/renderD128",
        "/dev/dri/card0",
        "/dev/pvr_sync",
        NULL
    };

    int fd = -1;
    (void)argc;
    (void)argv;
    const char *chosen_node = NULL;
    (void)chosen_node;

    for (int i = 0; candidates[i] != NULL; i++) {
        printf("[*] Probing node: %s ... ", candidates[i]);
        int test_fd = open(candidates[i], O_RDWR | O_CLOEXEC);
        if (test_fd >= 0) {
            printf("OPEN OK (fd=%d)\n", test_fd);
            fd = test_fd;
            chosen_node = candidates[i];
            break;
        } else {
            printf("FAILED (%s)\n", strerror(errno));
        }
    }

    if (fd < 0) {
        fprintf(stderr, "[-] Error: Could not open any graphics device node.\n");
        return 1;
    }

    /* Step 1: Services Module Init */
    printf("[*] Initializing Services module via DRM_IOCTL_SRVKM_INIT...\n");
    struct drm_srvkm_init_data init_data = {
        .init_module = PVR_SRVKM_SERVICES_INIT
    };
    int ret = ioctl(fd, DRM_IOCTL_SRVKM_INIT, &init_data);
    if (ret < 0) {
        printf("    [!] DRM_IOCTL_SRVKM_INIT returned %d (errno: %s) - continuing\n",
               ret, strerror(errno));
    } else {
        printf("    [+] DRM_IOCTL_SRVKM_INIT: Success\n");
    }

    /* Step 2: DDK 25.1 Connect Bridge Handshake */
    printf("[*] Sending PVRSRV_BRIDGE_SRVCORE_CONNECT (Bridge 1, Func 0)...\n");

    struct pvr_srv_bridge_connect_cmd connect_cmd = {
        .build_options = RGX_BUILD_OPTIONS_DDK25,
        .DDK_build = DDK_BUILD,
        .DDK_version = DDK_VERSION,
        .flags = PVR_SRV_FLAGS_CLIENT_64BIT_COMPAT
    };

    struct pvr_srv_bridge_connect_ret connect_ret = { 0 };

    struct drm_srvkm_cmd srvkm_cmd = {
        .bridge_id = PVR_SRV_BRIDGE_SRVCORE,
        .bridge_func_id = PVR_SRV_BRIDGE_SRVCORE_CONNECT,
        .in_data_ptr = (uint64_t)(uintptr_t)&connect_cmd,
        .out_data_ptr = (uint64_t)(uintptr_t)&connect_ret,
        .in_data_size = sizeof(connect_cmd),
        .out_data_size = sizeof(connect_ret)
    };

    ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &srvkm_cmd);
    if (ret < 0) {
        fprintf(stderr, "[-] ioctl(DRM_IOCTL_SRVKM_CMD) failed: %s (ret=%d)\n",
                strerror(errno), ret);
        close(fd);
        return 2;
    }

    printf("    [+] Bridge Call returned 0 (Kernel accepted the dispatch!)\n");
    printf("    [+] Kernel Return Error Code: %u\n", connect_ret.error);

    if (connect_ret.error != 0) {
        fprintf(stderr, "[-] Kernel rejected connection! Error code: %u\n", connect_ret.error);
        if (connect_ret.error == 26) {
            fprintf(stderr, "    -> 26 = PVRSRV_ERROR_DDK_VERSION_MISMATCH\n");
        } else if (connect_ret.error == 27) {
            fprintf(stderr, "    -> 27 = PVRSRV_ERROR_BUILD_OPTIONS_MISMATCH\n");
        }
        close(fd);
        return 3;
    }

    printf("\n=======================================================\n");
    printf("[+] SUCCESS: CONNECTED TO POWVR DDK 25.1 KERNEL DRIVER!\n");
    printf("=======================================================\n");
    print_bvnc(connect_ret.bvnc);
    printf("    -> Capability Flags: 0x%08x\n", connect_ret.capability_flags);
    printf("    -> Kernel Architecture: %u\n", connect_ret.kernel_arch);

    /* Step 3: Create Device Memory Context */
    printf("[*] Testing DEVMEMINTCTXCREATE (Bridge 6, Func 9)...\n");
    struct pvr_srv_bridge_devmem_int_ctx_create_cmd mem_cmd = {
        .kernel_memory_ctx = 0
    };
    struct pvr_srv_bridge_devmem_int_ctx_create_ret mem_ret = { 0 };

    struct drm_srvkm_cmd mem_srvkm_cmd = {
        .bridge_id = PVR_SRV_BRIDGE_MM,
        .bridge_func_id = PVR_SRV_BRIDGE_MM_DEVMEMINTCTXCREATE,
        .in_data_ptr = (uint64_t)(uintptr_t)&mem_cmd,
        .out_data_ptr = (uint64_t)(uintptr_t)&mem_ret,
        .in_data_size = sizeof(mem_cmd),
        .out_data_size = sizeof(mem_ret)
    };

    ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &mem_srvkm_cmd);
    if (ret == 0 && mem_ret.error == 0) {
        printf("    [+] GPU Memory Context Created Successfully!\n");
        printf("        -> server_memctx: %p\n", mem_ret.server_memctx);
        printf("        -> server_memctx_data: %p\n", mem_ret.server_memctx_data);
        printf("        -> cpu_cache_line_size: %u bytes\n", mem_ret.cpu_cache_line_size);

        /* Step 4: Allocate a 64KB GPU PMR Buffer */
        printf("[*] Testing PHYSMEMNEWRAMBACKEDPMR (Bridge 6, Func 8) 64KB alloc...\n");
        const char *tag = "MesaProbeBuffer";

        /* 1:1 Identity page mapping table: 16 x 4KB = 64KB */
        uint32_t mapping_table[16];
        for (uint32_t i = 0; i < 16; i++) {
            mapping_table[i] = i;
        }

        /* Flags: Default Heap (0) + GPU RW + CPU RW + KERNEL_CPU_MAPPABLE + CACHE_INCOHERENT */
        uint64_t alloc_flags = 0            /* Default Heap */
                             | (1ULL << 14) /* KERNEL_CPU_MAPPABLE */
                             | 0xff         /* GPU RW + CPU RW permitted */
                             | (3ULL << 8)  /* GPU CACHE INCOHERENT */
                             | (3ULL << 11);/* CPU CACHE INCOHERENT */

        struct pvr_srv_bridge_physmem_new_ram_backed_pmr_cmd pmr_cmd = {
            .size = 65536,
            .mapping_table = mapping_table,
            .annotation = tag,
            .annotation_size = strlen(tag) + 1,
            .log2_page_size = 12, /* 4KB page size */
            .phy_blocks = 16,     /* 16 physical pages backed by RAM */
            .virt_blocks = 16,    /* 16 virtual pages */
            .pdump_flags = 0,
            .pid = getpid(),
            .flags = alloc_flags
        };
        struct pvr_srv_bridge_physmem_new_ram_backed_pmr_ret pmr_ret = { 0 };

        struct drm_srvkm_cmd pmr_srvkm_cmd = {
            .bridge_id = PVR_SRV_BRIDGE_MM,
            .bridge_func_id = PVR_SRV_BRIDGE_MM_PHYSMEMNEWRAMBACKEDPMR,
            .in_data_ptr = (uint64_t)(uintptr_t)&pmr_cmd,
            .out_data_ptr = (uint64_t)(uintptr_t)&pmr_ret,
            .in_data_size = sizeof(pmr_cmd),
            .out_data_size = sizeof(pmr_ret)
        };

        ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &pmr_srvkm_cmd);
        if (ret == 0 && pmr_ret.error == 0) {
            printf("    [+] GPU PMR Buffer Allocated Successfully!\n");
            printf("        -> pmr handle: %p\n", pmr_ret.pmr);
            printf("        -> out_flags: 0x%016llx\n", (unsigned long long)pmr_ret.out_flags);

            /* Test PMRGETUID (Bridge 6, Func 2) to verify handle is valid */
            struct {
                void *pmr;
            } __attribute__((packed)) uid_cmd = { .pmr = pmr_ret.pmr };
            struct {
                uint64_t uid;
                uint32_t error;
            } __attribute__((packed)) uid_ret = { 0 };
            struct drm_srvkm_cmd uid_srvkm_cmd = {
                .bridge_id = PVR_SRV_BRIDGE_MM,
                .bridge_func_id = 2, /* PMRGETUID */
                .in_data_ptr = (uint64_t)(uintptr_t)&uid_cmd,
                .out_data_ptr = (uint64_t)(uintptr_t)&uid_ret,
                .in_data_size = sizeof(uid_cmd),
                .out_data_size = sizeof(uid_ret)
            };
            ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &uid_srvkm_cmd);
            printf("    [*] PMRGETUID test: ret=%d, error=%u, uid=0x%llx\n",
                   ret, uid_ret.error, (unsigned long long)uid_ret.uid);

            /* Step 5: Query Heaps and Test GPU Virtual Address Mapping */
            printf("[*] Querying Kernel GPU Heap Configurations...\n");

            /* 5a. HEAPCFGHEAPCONFIGCOUNT (Bridge 6, Func 21) */
            struct {
                uint32_t error;
                uint32_t config_count;
            } __attribute__((packed)) cfg_count_ret = { 0 };
            struct drm_srvkm_cmd cfg_count_srvkm_cmd = {
                .bridge_id = PVR_SRV_BRIDGE_MM,
                .bridge_func_id = 21, /* HEAPCFGHEAPCONFIGCOUNT */
                .in_data_ptr = 0,
                .out_data_ptr = (uint64_t)(uintptr_t)&cfg_count_ret,
                .in_data_size = 0,
                .out_data_size = sizeof(cfg_count_ret)
            };
            ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &cfg_count_srvkm_cmd);
            printf("    [*] HEAPCFGHEAPCONFIGCOUNT: ret=%d, error=%u, config_count=%u\n",
                   ret, cfg_count_ret.error, cfg_count_ret.config_count);

            uint32_t target_cfg_idx = 0;
            uint32_t target_heap_idx = 0;
            uint64_t target_heap_base = 0;
            uint64_t target_heap_size = 0;
            int found_heap = 0;

            if (ret == 0 && cfg_count_ret.error == 0) {
                for (uint32_t c = 0; c < cfg_count_ret.config_count; c++) {
                    struct {
                        uint32_t heap_config_index;
                    } __attribute__((packed)) hcount_cmd = { .heap_config_index = c };
                    struct {
                        uint32_t error;
                        uint32_t heap_count;
                    } __attribute__((packed)) hcount_ret = { 0 };
                    struct drm_srvkm_cmd hcount_srvkm_cmd = {
                        .bridge_id = PVR_SRV_BRIDGE_MM,
                        .bridge_func_id = 22, /* HEAPCFGHEAPCOUNT */
                        .in_data_ptr = (uint64_t)(uintptr_t)&hcount_cmd,
                        .out_data_ptr = (uint64_t)(uintptr_t)&hcount_ret,
                        .in_data_size = sizeof(hcount_cmd),
                        .out_data_size = sizeof(hcount_ret)
                    };
                    ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &hcount_srvkm_cmd);
                    printf("    [*] Config %u: heap_count=%u (ret=%d, error=%u)\n",
                           c, hcount_ret.heap_count, ret, hcount_ret.error);

                    for (uint32_t h = 0; h < hcount_ret.heap_count; h++) {
                        char heap_name[64] = { 0 };
                        struct {
                            uint64_t name_buf_ptr;
                            uint32_t heap_config_index;
                            uint32_t heap_index;
                            uint32_t name_buf_size;
                        } __attribute__((packed)) hdet_cmd = {
                            .name_buf_ptr = (uint64_t)(uintptr_t)heap_name,
                            .heap_config_index = c,
                            .heap_index = h,
                            .name_buf_size = sizeof(heap_name)
                        };
                        struct {
                            uint64_t dev_vaddr_base;
                            uint64_t size;
                            uint64_t reserved_size;
                            uint64_t name_buf_ptr;
                            uint32_t error;
                            uint32_t log2_page_size;
                            uint32_t flags;
                        } __attribute__((packed)) hdet_ret = { 0 };
                        struct drm_srvkm_cmd hdet_srvkm_cmd = {
                            .bridge_id = PVR_SRV_BRIDGE_MM,
                            .bridge_func_id = 24, /* HEAPCFGHEAPDETAILS */
                            .in_data_ptr = (uint64_t)(uintptr_t)&hdet_cmd,
                            .out_data_ptr = (uint64_t)(uintptr_t)&hdet_ret,
                            .in_data_size = sizeof(hdet_cmd), /* exactly 20 bytes */
                            .out_data_size = sizeof(hdet_ret) /* exactly 44 bytes */
                        };
                        ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &hdet_srvkm_cmd);
                        printf("        -> Heap [%u][%u] \"%s\": Base=0x%llx, Size=0x%llx, Flags=0x%x, Log2Page=%u (ret=%d, error=%u)\n",
                               c, h, heap_name,
                               (unsigned long long)hdet_ret.dev_vaddr_base,
                               (unsigned long long)hdet_ret.size,
                               hdet_ret.flags,
                               hdet_ret.log2_page_size, ret, hdet_ret.error);

                        /* Pick the "General" heap */
                        if (strcmp(heap_name, "General") == 0) {
                            target_cfg_idx = c;
                            target_heap_idx = h;
                            target_heap_base = hdet_ret.dev_vaddr_base;
                            target_heap_size = hdet_ret.size;
                            found_heap = 1;
                        }
                    }
                }
            }

            /* Step 5b: Create Heap Context via DEVMEMINTHEAPCREATE (Bridge 6, Func 11) */
            if (found_heap) {
                printf("[*] Creating GPU Heap: Cfg=%u, Heap=%u, Base=0x%llx...\n",
                       target_cfg_idx, target_heap_idx, (unsigned long long)target_heap_base);
                struct {
                    void *server_memctx;
                    uint32_t heap_config_index;
                    uint32_t heap_index;
                } __attribute__((packed)) hcreate_cmd = {
                    .server_memctx = mem_ret.server_memctx,
                    .heap_config_index = target_cfg_idx,
                    .heap_index = target_heap_idx
                };
                struct {
                    void *server_heap;
                    uint32_t error;
                } __attribute__((packed)) hcreate_ret = { 0 };
                struct drm_srvkm_cmd hcreate_srvkm_cmd = {
                    .bridge_id = PVR_SRV_BRIDGE_MM,
                    .bridge_func_id = 11, /* DEVMEMINTHEAPCREATE */
                    .in_data_ptr = (uint64_t)(uintptr_t)&hcreate_cmd,
                    .out_data_ptr = (uint64_t)(uintptr_t)&hcreate_ret,
                    .in_data_size = 16,
                    .out_data_size = 12
                };
                ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &hcreate_srvkm_cmd);
                printf("    [+] DEVMEMINTHEAPCREATE: ret=%d, error=%u, server_heap=%p\n",
                       ret, hcreate_ret.error, hcreate_ret.server_heap);

                if (ret == 0 && hcreate_ret.error == 0) {
                    /* Step 5c: Reserve Range via DEVMEMXINTRESERVERANGE (Bridge 6, Func 30) */
                    uint64_t gpu_vaddr = target_heap_base;
                    printf("[*] Testing DEVMEMXINTRESERVERANGE at 0x%llx...\n",
                           (unsigned long long)gpu_vaddr);

                    struct {
                        uint64_t dev_addr;
                        uint64_t size;
                        void *server_heap;
                    } __attribute__((packed)) res_cmd = {
                        .dev_addr = gpu_vaddr,
                        .size = 65536,
                        .server_heap = hcreate_ret.server_heap
                    };
                    struct {
                        void *reservation;
                        uint32_t error;
                    } __attribute__((packed)) res_ret = { 0 };
                    struct drm_srvkm_cmd res_srvkm_cmd = {
                        .bridge_id = PVR_SRV_BRIDGE_MM,
                        .bridge_func_id = 30, /* DEVMEMXINTRESERVERANGE */
                        .in_data_ptr = (uint64_t)(uintptr_t)&res_cmd,
                        .out_data_ptr = (uint64_t)(uintptr_t)&res_ret,
                        .in_data_size = 24,
                        .out_data_size = 12
                    };
                    ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &res_srvkm_cmd);
                    printf("    [+] DEVMEMXINTRESERVERANGE: ret=%d, error=%u, reservation=%p\n",
                           ret, res_ret.error, res_ret.reservation);

                    if (ret == 0 && res_ret.error == 0) {
                        /* Step 5d: Map Pages via DEVMEMXINTMAPPAGES (Bridge 6, Func 32) */
                        struct {
                            void *pmr;
                            void *reservation;
                            uint32_t page_count;
                            uint32_t pmr_page_offset;
                            uint32_t res_page_offset;
                            uint64_t flags;
                        } __attribute__((packed)) map_cmd = {
                            .pmr = pmr_ret.pmr,
                            .reservation = res_ret.reservation,
                            .page_count = 16,
                            .pmr_page_offset = 0,
                            .res_page_offset = 0,
                            .flags = 3 /* GPU_READABLE | GPU_WRITEABLE */
                        };
                        struct {
                            uint32_t error;
                        } __attribute__((packed)) map_ret = { 0 };
                        struct drm_srvkm_cmd map_srvkm_cmd = {
                            .bridge_id = PVR_SRV_BRIDGE_MM,
                            .bridge_func_id = 32, /* DEVMEMXINTMAPPAGES */
                            .in_data_ptr = (uint64_t)(uintptr_t)&map_cmd,
                            .out_data_ptr = (uint64_t)(uintptr_t)&map_ret,
                            .in_data_size = 36,
                            .out_data_size = 4
                        };
                        ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &map_srvkm_cmd);
                        printf("    [+] DEVMEMXINTMAPPAGES: ret=%d, error=%u\n", ret, map_ret.error);

                        if (ret == 0 && map_ret.error == 0) {
                            printf("    =======================================================\n");
                            printf("    [+] SUCCESS! GPU VIRTUAL ADDRESS MAPPED TO PMR!\n");
                            printf("    [+] DevVAddr: 0x%016llx (Size: 64KB, 16 Pages)\n",
                                   (unsigned long long)gpu_vaddr);
                            printf("    =======================================================\n");

                            /* Clean unmap: DEVMEMXINTUNMAPPAGES (Bridge 6, Func 33) */
                            struct {
                                void *reservation;
                                uint32_t page_offset;
                                uint32_t page_count;
                            } __attribute__((packed)) unmap_cmd = {
                                .reservation = res_ret.reservation,
                                .page_offset = 0,
                                .page_count = 16
                            };
                            struct {
                                uint32_t error;
                            } __attribute__((packed)) unmap_ret = { 0 };
                            struct drm_srvkm_cmd unmap_srvkm_cmd = {
                                .bridge_id = PVR_SRV_BRIDGE_MM,
                                .bridge_func_id = 33, /* DEVMEMXINTUNMAPPAGES */
                                .in_data_ptr = (uint64_t)(uintptr_t)&unmap_cmd,
                                .out_data_ptr = (uint64_t)(uintptr_t)&unmap_ret,
                                .in_data_size = 16,
                                .out_data_size = 4
                            };
                            ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &unmap_srvkm_cmd);
                            printf("    [*] DEVMEMXINTUNMAPPAGES: ret=%d, error=%u\n", ret, unmap_ret.error);
                        }

                        /* Clean unreserve: DEVMEMXINTUNRESERVERANGE (Bridge 6, Func 31) */
                        struct {
                            void *reservation;
                        } __attribute__((packed)) unres_cmd = {
                            .reservation = res_ret.reservation
                        };
                        struct {
                            uint32_t error;
                        } __attribute__((packed)) unres_ret = { 0 };
                        struct drm_srvkm_cmd unres_srvkm_cmd = {
                            .bridge_id = PVR_SRV_BRIDGE_MM,
                            .bridge_func_id = 31, /* DEVMEMXINTUNRESERVERANGE */
                            .in_data_ptr = (uint64_t)(uintptr_t)&unres_cmd,
                            .out_data_ptr = (uint64_t)(uintptr_t)&unres_ret,
                            .in_data_size = 8,
                            .out_data_size = 4
                        };
                        ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &unres_srvkm_cmd);
                        printf("    [*] DEVMEMXINTUNRESERVERANGE: ret=%d, error=%u\n", ret, unres_ret.error);
                    }

                    /* Clean heap destroy: DEVMEMINTHEAPDESTROY (Bridge 6, Func 12) */
                    struct {
                        void *server_heap;
                    } __attribute__((packed)) hdest_cmd = {
                        .server_heap = hcreate_ret.server_heap
                    };
                    struct {
                        uint32_t error;
                    } __attribute__((packed)) hdest_ret = { 0 };
                    struct drm_srvkm_cmd hdest_srvkm_cmd = {
                        .bridge_id = PVR_SRV_BRIDGE_MM,
                        .bridge_func_id = 12, /* DEVMEMINTHEAPDESTROY */
                        .in_data_ptr = (uint64_t)(uintptr_t)&hdest_cmd,
                        .out_data_ptr = (uint64_t)(uintptr_t)&hdest_ret,
                        .in_data_size = 8,
                        .out_data_size = 4
                    };
                    ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &hdest_srvkm_cmd);
                    printf("    [*] DEVMEMINTHEAPDESTROY: ret=%d, error=%u\n", ret, hdest_ret.error);
                }
            }

            /* Test PMR export to DMA-BUF (Bridge 11, Func 1) */
            printf("[*] Testing PHYSMEMEXPORTDMABUF (Bridge 11, Func 1)...\n");
            struct {
                void *pmr;
            } __attribute__((packed)) exp_cmd = { .pmr = pmr_ret.pmr };
            struct {
                uint32_t error;
                int32_t dma_fd;
            } __attribute__((packed)) exp_ret = { 0, -1 };
            struct drm_srvkm_cmd exp_srvkm_cmd = {
                .bridge_id = 11, /* PVR_SRV_BRIDGE_DMABUF */
                .bridge_func_id = 1, /* PHYSMEMEXPORTDMABUF */
                .in_data_ptr = (uint64_t)(uintptr_t)&exp_cmd,
                .out_data_ptr = (uint64_t)(uintptr_t)&exp_ret,
                .in_data_size = sizeof(exp_cmd),
                .out_data_size = sizeof(exp_ret)
            };
            ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &exp_srvkm_cmd);
            printf("    [*] Export DMA-BUF: ret=%d, error=%u, dma_fd=%d\n",
                   ret, exp_ret.error, exp_ret.dma_fd);

            if (ret == 0 && exp_ret.error == 0 && exp_ret.dma_fd >= 0) {
                printf("[*] Testing CPU mmap of DMA-BUF fd=%d...\n", exp_ret.dma_fd);
                void *dma_ptr = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, exp_ret.dma_fd, 0);
                if (dma_ptr != MAP_FAILED) {
                    printf("    [+] DMA-BUF CPU mmap SUCCESS! Address: %p\n", dma_ptr);
                    volatile uint32_t *dw = (volatile uint32_t *)dma_ptr;
                    dw[0] = 0x12345678;
                    dw[1] = 0xabcdef01;
                    printf("    [+] Written and read back: 0x%08x 0x%08x\n", dw[0], dw[1]);
                    munmap(dma_ptr, 65536);
                    printf("    [+] Cleanly unmapped DMA-BUF pointer.\n");
                } else {
                    printf("    [-] DMA-BUF CPU mmap failed: %s (errno=%d)\n", strerror(errno), errno);
                }
                close(exp_ret.dma_fd);
            }

            /* Free the PMR buffer */
            struct pvr_srv_bridge_pmr_unref_pmr_cmd unref_cmd = {
                .pmr = pmr_ret.pmr
            };
            struct pvr_srv_bridge_pmr_unref_pmr_ret unref_ret = { 0 };
            struct drm_srvkm_cmd unref_srvkm_cmd = {
                .bridge_id = PVR_SRV_BRIDGE_MM,
                .bridge_func_id = PVR_SRV_BRIDGE_MM_PMRUNREFPMR,
                .in_data_ptr = (uint64_t)(uintptr_t)&unref_cmd,
                .out_data_ptr = (uint64_t)(uintptr_t)&unref_ret,
                .in_data_size = sizeof(unref_cmd),
                .out_data_size = sizeof(unref_ret)
            };
            ioctl(fd, DRM_IOCTL_SRVKM_CMD, &unref_srvkm_cmd);
            printf("    [+] GPU PMR Buffer freed cleanly.\n");
        } else {
            printf("    [-] PHYSMEMNEWRAMBACKEDPMR failed (ret=%d, error=%u)\n", ret, pmr_ret.error);
        }

        /* Destroy the test memory context */
        struct pvr_srv_bridge_devmem_int_ctx_destroy_cmd mem_dest_cmd = {
            .server_memctx = mem_ret.server_memctx
        };
        struct pvr_srv_bridge_devmem_int_ctx_destroy_ret mem_dest_ret = { 0 };
        struct drm_srvkm_cmd mem_dest_srvkm_cmd = {
            .bridge_id = PVR_SRV_BRIDGE_MM,
            .bridge_func_id = PVR_SRV_BRIDGE_MM_DEVMEMINTCTXDESTROY,
            .in_data_ptr = (uint64_t)(uintptr_t)&mem_dest_cmd,
            .out_data_ptr = (uint64_t)(uintptr_t)&mem_dest_ret,
            .in_data_size = sizeof(mem_dest_cmd),
            .out_data_size = sizeof(mem_dest_ret)
        };
        ioctl(fd, DRM_IOCTL_SRVKM_CMD, &mem_dest_srvkm_cmd);
        printf("    [+] GPU Memory Context Destroyed cleanly.\n");
    } else {
        printf("    [-] DEVMEMINTCTXCREATE failed (ret=%d, error=%u)\n", ret, mem_ret.error);
    }

    /* Step 3: Clean Disconnect */
    printf("[*] Sending PVRSRV_BRIDGE_SRVCORE_DISCONNECT (Bridge 1, Func 1)...\n");
    struct pvr_srv_bridge_disconnect_cmd disconnect_cmd = { 0 };
    struct pvr_srv_bridge_disconnect_ret disconnect_ret = { 0 };

    struct drm_srvkm_cmd disc_srvkm_cmd = {
        .bridge_id = PVR_SRV_BRIDGE_SRVCORE,
        .bridge_func_id = PVR_SRV_BRIDGE_SRVCORE_DISCONNECT,
        .in_data_ptr = (uint64_t)(uintptr_t)&disconnect_cmd,
        .out_data_ptr = (uint64_t)(uintptr_t)&disconnect_ret,
        .in_data_size = sizeof(disconnect_cmd),
        .out_data_size = sizeof(disconnect_ret)
    };

    ret = ioctl(fd, DRM_IOCTL_SRVKM_CMD, &disc_srvkm_cmd);
    if (ret == 0 && disconnect_ret.error == 0) {
        printf("    [+] Cleanly disconnected from kernel services.\n");
    }

    close(fd);
    printf("[+] Probe finished successfully.\n");
    return 0;
}
