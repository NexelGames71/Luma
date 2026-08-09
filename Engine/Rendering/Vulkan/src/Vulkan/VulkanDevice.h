#pragma once

#include <optional>
#include <vector>

#include "Vulkan/VulkanCommon.h"

namespace Luma {

struct QueueFamilyIndices {
    std::optional<u32> graphics;
    std::optional<u32> present;
    bool Complete() const { return graphics.has_value() && present.has_value(); }
};

// Selects a physical device and creates the logical device + queues.
class VulkanDevice {
public:
    VulkanDevice(VkInstance instance, VkSurfaceKHR surface);
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    VkPhysicalDevice Physical() const { return m_physical; }
    VkDevice Logical() const { return m_device; }
    VkQueue GraphicsQueue() const { return m_graphicsQueue; }
    VkQueue PresentQueue() const { return m_presentQueue; }
    const QueueFamilyIndices& Queues() const { return m_indices; }

private:
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;  // borrowed
    VkPhysicalDevice m_physical = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    QueueFamilyIndices m_indices;
};

}  // namespace Luma
