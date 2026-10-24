#include <stdio.h>
#include <vulkan/vulkan.h>

int main(void) {
    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "GPUInfo",
        .applicationVersion = 1,
        .pEngineName = "None",
        .engineVersion = 1,
        .apiVersion = VK_API_VERSION_1_0
    };

    VkInstanceCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app
    };

    VkInstance instance;
    VkResult r = vkCreateInstance(&ci, NULL, &instance);
    if (r != VK_SUCCESS) {
        printf("vkCreateInstance failed: %d\n", r);
        return 1;
    }

    uint32_t count = 0;
    r = vkEnumeratePhysicalDevices(instance, &count, NULL);
    if (r != VK_SUCCESS || count == 0) {
        printf("No Vulkan devices found; result=%d count=%u\n",
               r, count);
        vkDestroyInstance(instance, NULL);
        return 1;
    }

    VkPhysicalDevice devices[16];
    if (count > 16) count = 16;

    r = vkEnumeratePhysicalDevices(instance, &count, devices);
    if (r != VK_SUCCESS) {
        printf("Device enumeration failed: %d\n", r);
        vkDestroyInstance(instance, NULL);
        return 1;
    }

    for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(devices[i], &p);

        printf("Device: %s\n", p.deviceName);
        printf("Vendor ID: 0x%04x\n", p.vendorID);
        printf("Device ID: 0x%04x\n", p.deviceID);
        printf("Driver version: %u\n", p.driverVersion);
        printf("Vulkan API: %u.%u.%u\n",
               VK_VERSION_MAJOR(p.apiVersion),
               VK_VERSION_MINOR(p.apiVersion),
               VK_VERSION_PATCH(p.apiVersion));
        printf("Device type: %u\n\n", p.deviceType);
    }

    vkDestroyInstance(instance, NULL);
    return 0;
}
