#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

int main(void) {
    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_0
    };
    VkInstanceCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app
    };
    VkInstance instance;
    VkResult r = vkCreateInstance(&ci, NULL, &instance);
    if (r != VK_SUCCESS) {
        printf("vkCreateInstance: %d\n", r);
        return 1;
    }

    uint32_t n = 0;
    vkEnumeratePhysicalDevices(instance, &n, NULL);
    if (!n) {
        puts("No Vulkan devices");
        vkDestroyInstance(instance, NULL);
        return 1;
    }

    VkPhysicalDevice dev[16];
    if (n > 16) n = 16;
    vkEnumeratePhysicalDevices(instance, &n, dev);

    for (uint32_t i = 0; i < n; i++) {
        VkPhysicalDeviceProperties2 p2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
        };
        VkPhysicalDeviceDriverProperties dp = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES
        };
        p2.pNext = &dp;
        vkGetPhysicalDeviceProperties2(dev[i], &p2);

        printf("\nDevice: %s\n", p2.properties.deviceName);
        printf("Driver name: %s\n", dp.driverName);
        printf("Driver info: %s\n", dp.driverInfo);
        printf("Conformance version: %u.%u.%u.%u\n",
               dp.conformanceVersion.major,
               dp.conformanceVersion.minor,
               dp.conformanceVersion.subminor,
               dp.conformanceVersion.patch);

        VkPhysicalDeviceDrmPropertiesEXT drm = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT
        };
        VkPhysicalDeviceProperties2 pdrm = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &drm
        };
        vkGetPhysicalDeviceProperties2(dev[i], &pdrm);

        printf("DRM primary available: %u\n", drm.hasPrimary);
        printf("DRM primary major/minor: %u:%u\n",
               drm.primaryMajor, drm.primaryMinor);
        printf("DRM render available: %u\n", drm.hasRender);
        printf("DRM render major/minor: %u:%u\n",
               drm.renderMajor, drm.renderMinor);
    }

    vkDestroyInstance(instance, NULL);
    return 0;
}
