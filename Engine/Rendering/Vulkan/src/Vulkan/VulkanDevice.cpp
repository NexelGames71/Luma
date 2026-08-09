#include "Vulkan/VulkanDevice.h"

#include <cstring>
#include <set>
#include <vector>

namespace Luma {
namespace {

const char* kDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device,
                                     VkSurfaceKHR surface) {
    QueueFamilyIndices indices;
    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (u32 i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
                                             &presentSupport);
        if (presentSupport) indices.present = i;
        if (indices.Complete()) break;
    }
    return indices;
}

bool SupportsExtensions(VkPhysicalDevice device) {
    u32 count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count,
                                         available.data());
    for (const char* required : kDeviceExtensions) {
        bool found = false;
        for (const auto& ext : available) {
            if (std::strcmp(ext.extensionName, required) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

bool SupportsDynamicRendering(VkPhysicalDevice device) {
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features13;
    vkGetPhysicalDeviceFeatures2(device, &features2);
    return features13.dynamicRendering == VK_TRUE;
}

int ScoreDevice(VkPhysicalDevice device, VkSurfaceKHR surface) {
    if (!FindQueueFamilies(device, surface).Complete()) return -1;
    if (!SupportsExtensions(device)) return -1;
    if (!SupportsDynamicRendering(device)) return -1;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);
    int score = 0;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
    score += static_cast<int>(props.limits.maxImageDimension2D);
    return score;
}

}  // namespace

VulkanDevice::VulkanDevice(VkInstance instance, VkSurfaceKHR surface)
    : m_surface(surface) {
    u32 count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    LUMA_ASSERT(count > 0, "no Vulkan physical devices found");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    int best = -1;
    for (VkPhysicalDevice device : devices) {
        int score = ScoreDevice(device, surface);
        if (score > best) {
            best = score;
            m_physical = device;
        }
    }
    LUMA_ASSERT(m_physical != VK_NULL_HANDLE, "no suitable GPU found");

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physical, &props);
    LUMA_LOG_INFO("Vulkan", "selected GPU: {} (driver {})", props.deviceName,
                  props.driverVersion);

    m_indices = FindQueueFamilies(m_physical, surface);

    std::set<u32> uniqueFamilies = {*m_indices.graphics, *m_indices.present};
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    float priority = 1.0f;
    for (u32 family : uniqueFamilies) {
        VkDeviceQueueCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueFamilyIndex = family;
        info.queueCount = 1;
        info.pQueuePriorities = &priority;
        queueInfos.push_back(info);
    }

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &features13;
    createInfo.queueCreateInfoCount = static_cast<u32>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.enabledExtensionCount =
        static_cast<u32>(std::size(kDeviceExtensions));
    createInfo.ppEnabledExtensionNames = kDeviceExtensions;

    VK_CHECK(vkCreateDevice(m_physical, &createInfo, nullptr, &m_device));

    vkGetDeviceQueue(m_device, *m_indices.graphics, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, *m_indices.present, 0, &m_presentQueue);
    LUMA_LOG_INFO("Vulkan", "logical device created (graphics={}, present={})",
                  *m_indices.graphics, *m_indices.present);
}

VulkanDevice::~VulkanDevice() {
    if (m_device) vkDestroyDevice(m_device, nullptr);
}

}  // namespace Luma
