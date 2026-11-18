/*
 * vk_device -- first end-to-end exercise of the BXM-8-256 Mesa driver.
 *
 * Android's libvulkan loader ignores VK_ICD_FILENAMES, so the only way to run
 * our driver on this phone is to be the loader: dlopen the .so by absolute
 * path, take vk_icdGetInstanceProcAddr, and call through the pointers the
 * driver itself returns. Mesa's dispatch tables are built by the driver in
 * vk_instance_init()/vk_device_init(), so no loader structures are needed.
 *
 * What it does, in order and stopping at the first failure:
 *
 *   1. create instance, enumerate physical devices
 *   2. print limits the Switch gate depends on (maxUniformBufferRange in
 *      particular, the M25 change) and the required features from
 *      VkPhysicalDeviceFeatures2 + Vulkan11/12 chains (the M24 change)
 *   3. create a device with those features enabled and one queue
 *   4. allocate a 64 KiB host-visible buffer, map it, clear it
 *   5. bind a 64 KiB UNIFORM descriptor -- larger than any 16 KiB cap, so this
 *      is the silicon check for M25
 *   6. record vkCmdFillBuffer, submit it, wait for idle, and read the pattern
 *      back (the first GPU work this driver has ever executed here)
 *   7. tear everything down in dependency order
 *
 * No images, no shaders, no swapchain, no surface: this is the transfer path
 * plus descriptor creation, which is the smallest set that still proves the
 * kernel bridge, the allocator and the command-stream submit/teardown work.
 *
 * Built by tools/device/build_smoke.sh, driven by tools/device/step3_device.sh.
 */

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <vulkan/vulkan.h>

#define DEFAULT_LIBRARY "/data/local/tmp/pvr-smoke/libvulkan_powervr_mesa.so"

#define BUFFER_SIZE (64u * 1024u)
#define FILL_PATTERN 0x5a17c0deu

#define CHECK(cond, ...)                                                      \
   do {                                                                       \
      if (!(cond)) {                                                          \
         fprintf(stderr, "FAIL: ");                                           \
         fprintf(stderr, __VA_ARGS__);                                        \
         fprintf(stderr, "\n");                                               \
         return 1;                                                            \
      }                                                                       \
   } while (0)

typedef PFN_vkVoidFunction (*PFN_icdGetInstanceProcAddr)(VkInstance instance,
                                                         const char *name);

static const char *result_name(VkResult r)
{
   switch (r) {
   case VK_SUCCESS:
      return "VK_SUCCESS";
   case VK_NOT_READY:
      return "VK_NOT_READY";
   case VK_TIMEOUT:
      return "VK_TIMEOUT";
   case VK_ERROR_OUT_OF_HOST_MEMORY:
      return "VK_ERROR_OUT_OF_HOST_MEMORY";
   case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
   case VK_ERROR_INITIALIZATION_FAILED:
      return "VK_ERROR_INITIALIZATION_FAILED";
   case VK_ERROR_DEVICE_LOST:
      return "VK_ERROR_DEVICE_LOST";
   case VK_ERROR_MEMORY_MAP_FAILED:
      return "VK_ERROR_MEMORY_MAP_FAILED";
   case VK_ERROR_LAYER_NOT_PRESENT:
      return "VK_ERROR_LAYER_NOT_PRESENT";
   case VK_ERROR_EXTENSION_NOT_PRESENT:
      return "VK_ERROR_EXTENSION_NOT_PRESENT";
   case VK_ERROR_FEATURE_NOT_PRESENT:
      return "VK_ERROR_FEATURE_NOT_PRESENT";
   case VK_ERROR_INCOMPATIBLE_DRIVER:
      return "VK_ERROR_INCOMPATIBLE_DRIVER";
   case VK_ERROR_TOO_MANY_OBJECTS:
      return "VK_ERROR_TOO_MANY_OBJECTS";
   case VK_ERROR_FORMAT_NOT_SUPPORTED:
      return "VK_ERROR_FORMAT_NOT_SUPPORTED";
   case VK_ERROR_OUT_OF_DATE_KHR:
      return "VK_ERROR_OUT_OF_DATE_KHR";
   default:
      return "VkResult?";
   }
}

#define RESULT(expr)                                                          \
   do {                                                                       \
      VkResult r_ = (expr);                                                   \
      if (r_ != VK_SUCCESS) {                                                 \
         fprintf(stderr, "FAIL: %s -> %s (%d)\n", #expr, result_name(r_),      \
                 (int)r_);                                                    \
         return 1;                                                            \
      }                                                                       \
   } while (0)

struct api {
   PFN_icdGetInstanceProcAddr icd_get_instance_proc_addr;
   PFN_vkCreateInstance CreateInstance;
   PFN_vkDestroyInstance DestroyInstance;
   PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
   PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
   PFN_vkGetPhysicalDeviceFeatures2 GetPhysicalDeviceFeatures2;
   PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
   PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
   PFN_vkCreateDevice CreateDevice;
   PFN_vkDestroyDevice DestroyDevice;
   PFN_vkGetDeviceQueue GetDeviceQueue;
   PFN_vkDeviceWaitIdle DeviceWaitIdle;
   PFN_vkCreateBuffer CreateBuffer;
   PFN_vkDestroyBuffer DestroyBuffer;
   PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
   PFN_vkAllocateMemory AllocateMemory;
   PFN_vkFreeMemory FreeMemory;
   PFN_vkBindBufferMemory BindBufferMemory;
   PFN_vkMapMemory MapMemory;
   PFN_vkUnmapMemory UnmapMemory;
   PFN_vkInvalidateMappedMemoryRanges InvalidateMappedMemoryRanges;
   PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout;
   PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout;
   PFN_vkCreateDescriptorPool CreateDescriptorPool;
   PFN_vkDestroyDescriptorPool DestroyDescriptorPool;
   PFN_vkAllocateDescriptorSets AllocateDescriptorSets;
   PFN_vkUpdateDescriptorSets UpdateDescriptorSets;
   PFN_vkCreateCommandPool CreateCommandPool;
   PFN_vkDestroyCommandPool DestroyCommandPool;
   PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
   PFN_vkBeginCommandBuffer BeginCommandBuffer;
   PFN_vkEndCommandBuffer EndCommandBuffer;
   PFN_vkCmdFillBuffer CmdFillBuffer;
   PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
   PFN_vkQueueSubmit QueueSubmit;
};

/* Instance- and device-level functions come back through the driver's own
 * vk_icdGetInstanceProcAddr once an instance exists: instance and physical
 * device functions directly, device functions as Mesa's vk_device_trampolines,
 * which dispatch through the vk_device the driver has just created. Fetching
 * any of them with a NULL instance returns NULL (vk_instance_get_proc_addr()
 * bails out there), so they are only loaded after vkCreateInstance. */
static void *proc_addr(const struct api *api, VkInstance instance,
                       const char *name)
{
   void *p = (void *)api->icd_get_instance_proc_addr(instance, name);
   if (!p) {
      fprintf(stderr, "FAIL: driver has no %s\n", name);
      exit(1);
   }
   return p;
}

static int load_api(void *lib, struct api *api)
{
   api->icd_get_instance_proc_addr =
      (PFN_icdGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
   if (!api->icd_get_instance_proc_addr) {
      fprintf(stderr, "FAIL: no vk_icdGetInstanceProcAddr: %s\n", dlerror());
      return 1;
   }

   api->CreateInstance = (PFN_vkCreateInstance)api->icd_get_instance_proc_addr(
      NULL, "vkCreateInstance");
   if (!api->CreateInstance) {
      fprintf(stderr, "FAIL: driver has no vkCreateInstance\n");
      return 1;
   }

   return 0;
}

static void load_device_api(const struct api *api, struct api *out,
                            VkInstance instance)
{
   *out = *api;

#define DEV(field, name)                                                      \
   out->field = (PFN_##name)proc_addr(api, instance, #name);

   DEV(EnumeratePhysicalDevices, vkEnumeratePhysicalDevices);
   DEV(GetPhysicalDeviceProperties, vkGetPhysicalDeviceProperties);
   DEV(GetPhysicalDeviceFeatures2, vkGetPhysicalDeviceFeatures2);
   DEV(GetPhysicalDeviceMemoryProperties, vkGetPhysicalDeviceMemoryProperties);
   DEV(GetPhysicalDeviceQueueFamilyProperties,
       vkGetPhysicalDeviceQueueFamilyProperties);
   DEV(CreateDevice, vkCreateDevice);
   DEV(DestroyInstance, vkDestroyInstance);
   DEV(DestroyDevice, vkDestroyDevice);
   DEV(GetDeviceQueue, vkGetDeviceQueue);
   DEV(DeviceWaitIdle, vkDeviceWaitIdle);
   DEV(CreateBuffer, vkCreateBuffer);
   DEV(DestroyBuffer, vkDestroyBuffer);
   DEV(GetBufferMemoryRequirements, vkGetBufferMemoryRequirements);
   DEV(AllocateMemory, vkAllocateMemory);
   DEV(FreeMemory, vkFreeMemory);
   DEV(BindBufferMemory, vkBindBufferMemory);
   DEV(MapMemory, vkMapMemory);
   DEV(UnmapMemory, vkUnmapMemory);
   DEV(InvalidateMappedMemoryRanges, vkInvalidateMappedMemoryRanges);
   DEV(CreateDescriptorSetLayout, vkCreateDescriptorSetLayout);
   DEV(DestroyDescriptorSetLayout, vkDestroyDescriptorSetLayout);
   DEV(CreateDescriptorPool, vkCreateDescriptorPool);
   DEV(DestroyDescriptorPool, vkDestroyDescriptorPool);
   DEV(AllocateDescriptorSets, vkAllocateDescriptorSets);
   DEV(UpdateDescriptorSets, vkUpdateDescriptorSets);
   DEV(CreateCommandPool, vkCreateCommandPool);
   DEV(DestroyCommandPool, vkDestroyCommandPool);
   DEV(AllocateCommandBuffers, vkAllocateCommandBuffers);
   DEV(BeginCommandBuffer, vkBeginCommandBuffer);
   DEV(EndCommandBuffer, vkEndCommandBuffer);
   DEV(CmdFillBuffer, vkCmdFillBuffer);
   DEV(CmdPipelineBarrier, vkCmdPipelineBarrier);
   DEV(QueueSubmit, vkQueueSubmit);
#undef DEV
}

static const char *device_type_name(VkPhysicalDeviceType type)
{
   switch (type) {
   case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      return "integrated";
   case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return "discrete";
   case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return "virtual";
   case VK_PHYSICAL_DEVICE_TYPE_CPU:
      return "cpu";
   default:
      return "other";
   }
}

struct switch_features {
   VkPhysicalDeviceFeatures2 core;
   VkPhysicalDeviceVulkan11Features v11;
   VkPhysicalDeviceVulkan12Features v12;
};

static void query_switch_features(const struct api *api, VkPhysicalDevice pdev,
                                  struct switch_features *out)
{
   memset(out, 0, sizeof(*out));
   out->v11 = (VkPhysicalDeviceVulkan11Features){
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
   };
   out->v12 = (VkPhysicalDeviceVulkan12Features){
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &out->v11,
   };
   out->core = (VkPhysicalDeviceFeatures2){
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &out->v12,
   };
   api->GetPhysicalDeviceFeatures2(pdev, &out->core);
}

static void print_switch_features(const struct switch_features *f)
{
   const VkPhysicalDeviceFeatures *c = &f->core.features;

   printf("      required-by-Eden features:\n");
   printf("        independentBlend                 %d\n", c->independentBlend);
   printf("        fillModeNonSolid                 %d\n", c->fillModeNonSolid);
   printf("        vertexPipelineStoresAndAtomics   %d\n",
          c->vertexPipelineStoresAndAtomics);
   printf("        shaderInt16                      %d\n", c->shaderInt16);
   printf("        shaderInt8 (1.2 chain)           %d\n", f->v12.shaderInt8);
   printf("        shaderFloat16 (1.2 chain)        %d\n", f->v12.shaderFloat16);
   printf("        storageBuffer8BitAccess          %d\n",
          f->v12.storageBuffer8BitAccess);
   printf("        uniformAndStorageBuffer8BitAccess %d\n",
          f->v12.uniformAndStorageBuffer8BitAccess);
   printf("        storageBuffer16BitAccess         %d\n",
          f->v11.storageBuffer16BitAccess);
   printf("        uniformAndStorageBuffer16BitAccess %d\n",
          f->v11.uniformAndStorageBuffer16BitAccess);
   printf("        keep-off storagePushConstant8/16 %d/%d, storageInputOutput16 %d\n",
          f->v12.storagePushConstant8, f->v11.storagePushConstant16,
          f->v11.storageInputOutput16);
}

/* --drm: report what each /dev/dri node actually is. The driver only accepts a
 * node whose DRM driver name is "pvrsrvkm" or "pvr" (pvr_winsys.h), so this is
 * the first thing to check when it enumerates no physical device. */
struct drm_version_local {
   int version_major;
   int version_minor;
   int version_patchlevel;
   size_t name_len;
   char *name;
   size_t date_len;
   char *date;
   size_t desc_len;
   char *desc;
};

#define DRM_IOCTL_VERSION_LOCAL _IOWR('d', 0x00, struct drm_version_local)

static int drm_probe(void)
{
   DIR *dir = opendir("/dev/dri");
   struct dirent *entry;
   int failures = 0;

   if (!dir) {
      printf("cannot open /dev/dri\n");
      return 1;
   }

   printf("DRM nodes under /dev/dri:\n");
   while ((entry = readdir(dir))) {
      char path[128];
      struct drm_version_local version = {0};
      char name[64] = {0};
      int fd;

      if (entry->d_name[0] == '.')
         continue;
      snprintf(path, sizeof(path), "/dev/dri/%s", entry->d_name);
      fd = open(path, O_RDWR | O_CLOEXEC);
      if (fd < 0) {
         printf("  %s: open failed (errno %d)\n", path, errno);
         failures++;
         continue;
      }
      version.name = name;
      version.name_len = sizeof(name) - 1;
      if (ioctl(fd, DRM_IOCTL_VERSION_LOCAL, &version) < 0) {
         printf("  %s: DRM_IOCTL_VERSION failed (errno %d)\n", path, errno);
         failures++;
      } else {
         printf("  %s: drm driver name \"%s\" (%d.%d.%d)\n", path, name,
                version.version_major, version.version_minor,
                version.version_patchlevel);
         if (strcmp(name, "pvr") == 0 || strcmp(name, "pvrsrvkm") == 0 ||
             strcmp(name, "powervr") == 0)
            printf("      ^ accepted by the driver's winsys check\n");
      }
      close(fd);
   }
   closedir(dir);

   if (failures)
      printf("\n%d node(s) unusable from this process\n", failures);
   return 0;
}

int main(int argc, char **argv)
{
   const char *library = argc > 1 ? argv[1] : DEFAULT_LIBRARY;

   if (argc > 1 && strcmp(argv[1], "--drm") == 0)
      return drm_probe();
   void *lib;
   struct api api, dev_api;
   VkInstance instance = VK_NULL_HANDLE;
   VkPhysicalDevice pdev = VK_NULL_HANDLE;
   VkDevice device = VK_NULL_HANDLE;
   VkQueue queue = VK_NULL_HANDLE;
   VkBuffer buffer = VK_NULL_HANDLE;
   VkDeviceMemory memory = VK_NULL_HANDLE;
   VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
   VkDescriptorPool pool = VK_NULL_HANDLE;
   VkDescriptorSet set = VK_NULL_HANDLE;
   VkCommandPool cmd_pool = VK_NULL_HANDLE;
   VkCommandBuffer cmd = VK_NULL_HANDLE;
   struct switch_features features;
   VkPhysicalDeviceProperties props;
   VkPhysicalDeviceMemoryProperties mem_props;
   VkQueueFamilyProperties queue_props[8];
   VkPhysicalDeviceFeatures2 device_features;
   VkPhysicalDeviceVulkan11Features device_f11;
   VkPhysicalDeviceVulkan12Features device_f12;
   uint32_t count = 0;
   uint32_t queue_count;
   uint32_t family = 0;
   uint32_t memory_type = UINT32_MAX;
   VkMemoryRequirements requirements;
   void *mapped = NULL;
   int result = 1;

   printf("vk_device: instance -> device -> buffer -> fill -> teardown\n");
   printf("  library: %s\n", library);
   fflush(stdout);

   lib = dlopen(library, RTLD_NOW | RTLD_LOCAL);
   if (!lib) {
      fprintf(stderr, "FAIL: dlopen: %s\n", dlerror());
      return 1;
   }

   if (load_api(lib, &api))
      return 1;

   /* 1. instance ------------------------------------------------------- */
   VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "vk_device",
      .applicationVersion = 1,
      .pEngineName = "bxm-research",
      .engineVersion = 1,
      /* 1.2: vkGetPhysicalDeviceFeatures2 is filtered out of the dispatch table
       * for a 1.0 instance, and the 1.2 feature chain needs 1.2 anyway. */
      .apiVersion = VK_API_VERSION_1_2,
   };
   VkInstanceCreateInfo instance_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
   };

   RESULT(api.CreateInstance(&instance_info, NULL, &instance));
   printf("  [1] instance created\n");
   fflush(stdout);

   load_device_api(&api, &dev_api, instance);

   /* 2. physical device ------------------------------------------------ */
   RESULT(dev_api.EnumeratePhysicalDevices(instance, &count, NULL));
   CHECK(count > 0,
         "the driver enumerated no physical device. Either no render node "
         "reports the pvrsrvkm DRM name, or the winsys rejected every node "
         "(see the mesa logcat lines above).");
   VkPhysicalDevice devices[8];
   if (count > 8)
      count = 8;
   RESULT(dev_api.EnumeratePhysicalDevices(instance, &count, devices));
   pdev = devices[0];
   printf("  [2] %u physical device(s); using [0]\n", count);

   dev_api.GetPhysicalDeviceProperties(pdev, &props);
   dev_api.GetPhysicalDeviceMemoryProperties(pdev, &mem_props);
   query_switch_features(&dev_api, pdev, &features);

   printf("      %s (%s)\n", props.deviceName,
          device_type_name(props.deviceType));
   printf("      apiVersion %u.%u.%u, driverVersion 0x%08x\n",
          VK_API_VERSION_MAJOR(props.apiVersion),
          VK_API_VERSION_MINOR(props.apiVersion),
          VK_API_VERSION_PATCH(props.apiVersion), props.driverVersion);
   printf("      limits.maxUniformBufferRange = %" PRIu32 " bytes (%" PRIu32
          " KiB)\n",
          props.limits.maxUniformBufferRange,
          props.limits.maxUniformBufferRange >> 10);
   printf("      limits.maxStorageBufferRange = %" PRIu32 " bytes\n",
          props.limits.maxStorageBufferRange);
   print_switch_features(&features);

   CHECK(props.limits.maxUniformBufferRange >= BUFFER_SIZE,
         "maxUniformBufferRange is %u, below the %u-byte binding this test "
         "needs (M25 regression)",
         props.limits.maxUniformBufferRange, BUFFER_SIZE);

   /* 3. device --------------------------------------------------------- */
   queue_count = 8;
   dev_api.GetPhysicalDeviceQueueFamilyProperties(pdev, &queue_count,
                                                  queue_props);
   CHECK(queue_count > 0, "no queue families");
   for (uint32_t i = 0; i < queue_count; i++) {
      printf("      queue family %u: flags 0x%x, %u queue(s)\n", i,
             queue_props[i].queueFlags, queue_props[i].queueCount);
      if ((queue_props[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT |
                                        VK_QUEUE_TRANSFER_BIT)) &&
          queue_props[i].queueCount > 0)
         family = i;
   }

   /* Enable exactly what the emulator requires of us, taken from what the
    * driver just reported, so a feature that is 0 here stays 0 in the chain
    * instead of turning into a validation error. */
   device_f12 = (VkPhysicalDeviceVulkan12Features){
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .shaderInt8 = features.v12.shaderInt8,
      .shaderFloat16 = features.v12.shaderFloat16,
      .storageBuffer8BitAccess = features.v12.storageBuffer8BitAccess,
      .uniformAndStorageBuffer8BitAccess =
         features.v12.uniformAndStorageBuffer8BitAccess,
   };
   device_f11 = (VkPhysicalDeviceVulkan11Features){
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .storageBuffer16BitAccess = features.v11.storageBuffer16BitAccess,
      .uniformAndStorageBuffer16BitAccess =
         features.v11.uniformAndStorageBuffer16BitAccess,
   };
   device_features = (VkPhysicalDeviceFeatures2){
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &device_f12,
      .features =
         {
            .independentBlend = features.core.features.independentBlend,
            .fillModeNonSolid = features.core.features.fillModeNonSolid,
            .vertexPipelineStoresAndAtomics =
               features.core.features.vertexPipelineStoresAndAtomics,
            .shaderInt16 = features.core.features.shaderInt16,
         },
   };
   device_f12.pNext = &device_f11;

   float priority = 1.0f;
   VkDeviceQueueCreateInfo queue_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = family,
      .queueCount = 1,
      .pQueuePriorities = &priority,
   };
   VkDeviceCreateInfo device_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &device_features,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queue_info,
   };

   RESULT(dev_api.CreateDevice(pdev, &device_info, NULL, &device));
   printf("  [3] device created on queue family %u\n", family);
   fflush(stdout);
   dev_api.GetDeviceQueue(device, family, 0, &queue);

   /* 4. buffer + memory ------------------------------------------------ */
   VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = BUFFER_SIZE,
      .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
   };
   RESULT(dev_api.CreateBuffer(device, &buffer_info, NULL, &buffer));
   dev_api.GetBufferMemoryRequirements(device, buffer, &requirements);
   printf("  [4] buffer %u B: memoryTypeBits 0x%x, size %" PRIu64
          ", alignment %" PRIu64 "\n",
          BUFFER_SIZE, requirements.memoryTypeBits,
          (uint64_t)requirements.size, (uint64_t)requirements.alignment);

   for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
      const VkMemoryType *type = &mem_props.memoryTypes[i];
      if (!(requirements.memoryTypeBits & (1u << i)))
         continue;
      if ((type->propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
          (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
         memory_type = i;
         break;
      }
   }
   CHECK(memory_type != UINT32_MAX,
         "no host-visible coherent memory type for a %u-byte buffer",
         BUFFER_SIZE);

   VkMemoryAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = requirements.size,
      .memoryTypeIndex = memory_type,
   };
   RESULT(dev_api.AllocateMemory(device, &alloc_info, NULL, &memory));
   RESULT(dev_api.BindBufferMemory(device, buffer, memory, 0));
   RESULT(dev_api.MapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &mapped));
   memset(mapped, 0, BUFFER_SIZE);
   printf("      bound to memory type %u (heap %u), %" PRIu64 " MiB heap\n",
          memory_type, mem_props.memoryTypes[memory_type].heapIndex,
          (uint64_t)(mem_props.memoryHeaps[mem_props.memoryTypes[memory_type]
                                              .heapIndex]
                        .size >>
                     20));

   /* 5. a 64 KiB uniform descriptor -- the M25 silicon check ------------ */
   VkDescriptorSetLayoutBinding binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_ALL,
   };
   VkDescriptorSetLayoutCreateInfo layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &binding,
   };
   RESULT(dev_api.CreateDescriptorSetLayout(device, &layout_info, NULL,
                                            &set_layout));

   VkDescriptorPoolSize pool_size = {
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
   };
   VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1,
      .poolSizeCount = 1,
      .pPoolSizes = &pool_size,
   };
   RESULT(dev_api.CreateDescriptorPool(device, &pool_info, NULL, &pool));

   VkDescriptorSetAllocateInfo set_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &set_layout,
   };
   RESULT(dev_api.AllocateDescriptorSets(device, &set_info, &set));

   VkDescriptorBufferInfo buffer_desc = {
      .buffer = buffer,
      .offset = 0,
      .range = BUFFER_SIZE,
   };
   VkWriteDescriptorSet write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = set,
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo = &buffer_desc,
   };
   dev_api.UpdateDescriptorSets(device, 1, &write, 0, NULL);
   printf("  [5] descriptor set holds a %u-byte UNIFORM range (M25)\n",
          BUFFER_SIZE);
   fflush(stdout);

   /* 6. record, submit, read back -------------------------------------- */
   VkCommandPoolCreateInfo cmd_pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = family,
   };
   RESULT(dev_api.CreateCommandPool(device, &cmd_pool_info, NULL, &cmd_pool));
   VkCommandBufferAllocateInfo cmd_alloc = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = cmd_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
   };
   RESULT(dev_api.AllocateCommandBuffers(device, &cmd_alloc, &cmd));

   VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
   };
   RESULT(dev_api.BeginCommandBuffer(cmd, &begin_info));
   dev_api.CmdFillBuffer(cmd, buffer, 0, BUFFER_SIZE, FILL_PATTERN);

   VkBufferMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = buffer,
      .offset = 0,
      .size = VK_WHOLE_SIZE,
   };
   dev_api.CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1,
                             &barrier, 0, NULL);
   RESULT(dev_api.EndCommandBuffer(cmd));

   VkSubmitInfo submit = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
   };
   RESULT(dev_api.QueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
   printf("  [6] vkCmdFillBuffer submitted\n");
   fflush(stdout);
   RESULT(dev_api.DeviceWaitIdle(device));

   VkMappedMemoryRange invalidate = {
      .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
      .memory = memory,
      .offset = 0,
      .size = VK_WHOLE_SIZE,
   };
   RESULT(dev_api.InvalidateMappedMemoryRanges(device, 1, &invalidate));

   uint32_t *words = mapped;
   uint32_t bad = 0;
   for (uint32_t i = 0; i < BUFFER_SIZE / sizeof(uint32_t); i++) {
      if (words[i] != FILL_PATTERN)
         bad++;
   }
   printf("      read back: word[0] = 0x%08x, word[last] = 0x%08x, %u/%u "
          "words wrong\n",
          words[0], words[BUFFER_SIZE / sizeof(uint32_t) - 1], bad,
          BUFFER_SIZE / (uint32_t)sizeof(uint32_t));
   CHECK(bad == 0, "the GPU wrote %u wrong words -- the fill did not reach the "
                  "buffer",
         bad);
   CHECK(words[0] == FILL_PATTERN && words[BUFFER_SIZE / 4 - 1] == FILL_PATTERN,
         "fill pattern missing from the mapped range");

   /* 7. teardown ------------------------------------------------------- */
   RESULT(dev_api.DeviceWaitIdle(device));
   dev_api.UnmapMemory(device, memory);
   mapped = NULL;
   dev_api.DestroyCommandPool(device, cmd_pool, NULL);
   cmd_pool = VK_NULL_HANDLE;
   dev_api.DestroyDescriptorPool(device, pool, NULL);
   pool = VK_NULL_HANDLE;
   set = VK_NULL_HANDLE;
   dev_api.DestroyDescriptorSetLayout(device, set_layout, NULL);
   set_layout = VK_NULL_HANDLE;
   dev_api.DestroyBuffer(device, buffer, NULL);
   buffer = VK_NULL_HANDLE;
   dev_api.FreeMemory(device, memory, NULL);
   memory = VK_NULL_HANDLE;
   dev_api.DestroyDevice(device, NULL);
   device = VK_NULL_HANDLE;
   dev_api.DestroyInstance(instance, NULL);
   instance = VK_NULL_HANDLE;
   dlclose(lib);
   printf("  [7] torn down cleanly\n");

   printf("\nOK: instance, device, queue, buffer, %u-byte UBO descriptor, "
          "submit and teardown all worked on silicon\n",
          BUFFER_SIZE);
   result = 0;
   return result;
}
