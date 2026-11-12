/*
 * vk_enum -- minimal, read-only Vulkan enumeration for the BXM-8-256 bring-up.
 *
 * This is the on-device equivalent of `vulkaninfo --summary`. It creates an
 * instance, enumerates physical devices, and prints what the loader sees:
 * device name, type, API/driver versions, one representative limit and the
 * memory heaps. It creates no images, buffers, queues or command buffers and
 * submits no work, so it cannot reach the command-stream teardown path that
 * caused the earlier kernel Oops in RemoveAndFreeStreamNode.
 *
 * Built by tools/device/build_smoke.sh, driven by tools/device/step2_smoke.sh.
 */

#include <dlfcn.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>

#define MAX_DEVICES 8
#define MAX_HEAPS 16

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

int main(void)
{
   void *lib;
   PFN_vkGetInstanceProcAddr get_proc;
   PFN_vkEnumerateInstanceVersion enumerate_instance_version;
   PFN_vkCreateInstance create_instance;
   PFN_vkEnumeratePhysicalDevices enumerate_physical_devices;
   PFN_vkGetPhysicalDeviceProperties get_properties;
   PFN_vkGetPhysicalDeviceMemoryProperties get_memory_properties;
   PFN_vkDestroyInstance destroy_instance = NULL;
   VkInstance instance = VK_NULL_HANDLE;
   VkInstanceCreateInfo instance_info;
   VkApplicationInfo app_info;
   VkPhysicalDevice devices[MAX_DEVICES];
   uint32_t count = MAX_DEVICES;
   uint32_t instance_version = VK_API_VERSION_1_0;
   VkResult result;

   printf("vk_enum: read-only physical device enumeration\n");
   printf("  VK_ICD_FILENAMES=%s\n",
          getenv("VK_ICD_FILENAMES") ? getenv("VK_ICD_FILENAMES") : "(unset)");
   printf("  VK_DRIVER_FILES=%s\n",
          getenv("VK_DRIVER_FILES") ? getenv("VK_DRIVER_FILES") : "(unset)");
   printf("  VK_LOADER_DEBUG=%s\n",
          getenv("VK_LOADER_DEBUG") ? getenv("VK_LOADER_DEBUG") : "(unset)");

   lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
   if (!lib)
      lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
   if (!lib) {
      fprintf(stderr, "FAIL: cannot dlopen libvulkan: %s\n", dlerror());
      return 1;
   }

   get_proc = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
   if (!get_proc) {
      fprintf(stderr, "FAIL: libvulkan has no vkGetInstanceProcAddr\n");
      return 1;
   }

   enumerate_instance_version =
      (PFN_vkEnumerateInstanceVersion)get_proc(NULL,
                                               "vkEnumerateInstanceVersion");
   if (enumerate_instance_version)
      enumerate_instance_version(&instance_version);
   printf("  loader instance version: %u.%u.%u\n",
          VK_API_VERSION_MAJOR(instance_version),
          VK_API_VERSION_MINOR(instance_version),
          VK_API_VERSION_PATCH(instance_version));

   create_instance = (PFN_vkCreateInstance)get_proc(NULL, "vkCreateInstance");
   if (!create_instance) {
      fprintf(stderr, "FAIL: no vkCreateInstance\n");
      return 1;
   }

   app_info = (VkApplicationInfo){
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "vk_enum",
      .applicationVersion = 1,
      .pEngineName = "bxm-research",
      .engineVersion = 1,
      .apiVersion = VK_API_VERSION_1_0,
   };
   instance_info = (VkInstanceCreateInfo){
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
   };

   result = create_instance(&instance_info, NULL, &instance);
   if (result != VK_SUCCESS) {
      fprintf(stderr, "FAIL: vkCreateInstance returned %d\n", result);
      return 1;
   }

   enumerate_physical_devices = (PFN_vkEnumeratePhysicalDevices)get_proc(
      instance, "vkEnumeratePhysicalDevices");
   get_properties = (PFN_vkGetPhysicalDeviceProperties)get_proc(
      instance, "vkGetPhysicalDeviceProperties");
   get_memory_properties = (PFN_vkGetPhysicalDeviceMemoryProperties)get_proc(
      instance, "vkGetPhysicalDeviceMemoryProperties");
   destroy_instance =
      (PFN_vkDestroyInstance)get_proc(instance, "vkDestroyInstance");

   if (!enumerate_physical_devices || !get_properties ||
       !get_memory_properties) {
      fprintf(stderr, "FAIL: instance entry points missing\n");
      if (destroy_instance)
         destroy_instance(instance, NULL);
      return 1;
   }

   result = enumerate_physical_devices(instance, &count, NULL);
   if (result != VK_SUCCESS) {
      fprintf(stderr, "FAIL: vkEnumeratePhysicalDevices(count) returned %d\n",
              result);
      if (destroy_instance)
         destroy_instance(instance, NULL);
      return 1;
   }

   printf("  physical devices: %u\n", count);
   if (count == 0) {
      fprintf(stderr,
              "FAIL: the loader sees no devices. Check that "
              "VK_ICD_FILENAMES points at powervr_icd.aarch64.json and that "
              "its library_path resolves.\n");
      if (destroy_instance)
         destroy_instance(instance, NULL);
      return 1;
   }

   if (count > MAX_DEVICES)
      count = MAX_DEVICES;

   result = enumerate_physical_devices(instance, &count, devices);
   if (result != VK_SUCCESS) {
      fprintf(stderr, "FAIL: vkEnumeratePhysicalDevices returned %d\n",
              result);
      if (destroy_instance)
         destroy_instance(instance, NULL);
      return 1;
   }

   for (uint32_t i = 0; i < count; i++) {
      VkPhysicalDeviceProperties props;
      VkPhysicalDeviceMemoryProperties mem;
      uint32_t heaps;

      get_properties(devices[i], &props);
      get_memory_properties(devices[i], &mem);

      printf("\n  [%u] %s\n", i, props.deviceName);
      printf("      type:          %s (%d)\n",
             device_type_name(props.deviceType), props.deviceType);
      printf("      apiVersion:    %u.%u.%u\n",
             VK_API_VERSION_MAJOR(props.apiVersion),
             VK_API_VERSION_MINOR(props.apiVersion),
             VK_API_VERSION_PATCH(props.apiVersion));
      printf("      driverVersion: %u.%u.%u (raw 0x%08x)\n",
             VK_API_VERSION_MAJOR(props.driverVersion),
             VK_API_VERSION_MINOR(props.driverVersion),
             VK_API_VERSION_PATCH(props.driverVersion),
             props.driverVersion);
      printf("      maxImageDimension2D: %u\n",
             props.limits.maxImageDimension2D);

      heaps = mem.memoryHeapCount > MAX_HEAPS ? MAX_HEAPS : mem.memoryHeapCount;
      printf("      memory heaps:  %u\n", heaps);
      for (uint32_t h = 0; h < heaps; h++) {
         printf("        heap %u: %" PRIu64 " MiB%s\n", h,
                (uint64_t)(mem.memoryHeaps[h].size >> 20),
                (mem.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                   ? " (device local)"
                   : "");
      }
   }

   if (destroy_instance)
      destroy_instance(instance, NULL);

   printf("\nOK: %u physical device(s) enumerated, no work submitted\n", count);
   return 0;
}
