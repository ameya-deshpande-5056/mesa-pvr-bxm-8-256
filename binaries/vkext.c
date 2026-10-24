#include <stdio.h>
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
    r = vkEnumeratePhysicalDevices(instance, &n, NULL);
    if (r != VK_SUCCESS || n == 0) {
        printf("No devices: %d\n", r);
        vkDestroyInstance(instance, NULL);
        return 1;
    }

    VkPhysicalDevice dev[16];
    if (n > 16) n = 16;
    vkEnumeratePhysicalDevices(instance, &n, dev);

    for (uint32_t i = 0; i < n; i++) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(dev[i], &p);
        printf("\nDevice: %s\n", p.deviceName);

        uint32_t ec = 0;
        r = vkEnumerateDeviceExtensionProperties(dev[i], NULL, &ec, NULL);
        if (r != VK_SUCCESS) {
            printf("Extension query failed: %d\n", r);
            continue;
        }

        VkExtensionProperties exts[512];
        if (ec > 512) ec = 512;
        r = vkEnumerateDeviceExtensionProperties(dev[i], NULL, &ec, exts);
        if (r != VK_SUCCESS) {
            printf("Extension list failed: %d\n", r);
            continue;
        }

        printf("Device extensions (%u):\n", ec);
        for (uint32_t j = 0; j < ec; j++)
            printf("  %s\n", exts[j].extensionName);
    }

    vkDestroyInstance(instance, NULL);
    return 0;
}
