#pragma once

#include <vector>

#include "Vulkan/VulkanCommon.h"
#include "Vulkan/VulkanDevice.h"

namespace Luma {

// Owns the swapchain, its images and image views. Recreated on resize.
class VulkanSwapchain {
public:
    VulkanSwapchain(VulkanDevice& device, VkSurfaceKHR surface, u32 width,
                    u32 height, bool vsync);
    ~VulkanSwapchain();

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    // Tears down and rebuilds at the new size (device must be idle).
    void Recreate(u32 width, u32 height);

    VkSwapchainKHR Handle() const { return m_swapchain; }
    VkFormat Format() const { return m_format; }
    VkExtent2D Extent() const { return m_extent; }
    u32 ImageCount() const { return static_cast<u32>(m_images.size()); }
    VkImage Image(u32 i) const { return m_images[i]; }
    VkImageView ImageView(u32 i) const { return m_imageViews[i]; }

private:
    void Build(u32 width, u32 height);
    void Destroy();

    VulkanDevice& m_device;
    VkSurfaceKHR m_surface;  // borrowed
    bool m_vsync;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
};

}  // namespace Luma
