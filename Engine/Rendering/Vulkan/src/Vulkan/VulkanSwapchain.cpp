#include "Vulkan/VulkanSwapchain.h"

#include <algorithm>
#include <limits>

namespace Luma {
namespace {

VkSurfaceFormatKHR ChooseFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    // Prefer a UNORM format so UI colors picked in sRGB bytes display as-is (no
    // implicit linear->sRGB encode). 3D gamma is handled later via an HDR target
    // + tonemap into the swapchain.
    for (const auto& f : formats) {
        if ((f.format == VK_FORMAT_B8G8R8A8_UNORM ||
             f.format == VK_FORMAT_R8G8B8A8_UNORM) &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats.front();
}

VkPresentModeKHR ChoosePresentMode(
    const std::vector<VkPresentModeKHR>& modes, bool vsync) {
    if (!vsync) {
        for (auto m : modes) {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
        }
        for (auto m : modes) {
            if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) return m;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;  // always available (vsync)
}

}  // namespace

VulkanSwapchain::VulkanSwapchain(VulkanDevice& device, VkSurfaceKHR surface,
                                 u32 width, u32 height, bool vsync)
    : m_device(device), m_surface(surface), m_vsync(vsync) {
    Build(width, height);
}

VulkanSwapchain::~VulkanSwapchain() { Destroy(); }

void VulkanSwapchain::Recreate(u32 width, u32 height) {
    Destroy();
    Build(width, height);
}

void VulkanSwapchain::Build(u32 width, u32 height) {
    VkPhysicalDevice phys = m_device.Physical();

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, m_surface, &caps);

    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, m_surface, &formatCount,
                                         formats.data());

    u32 modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, m_surface, &modeCount,
                                              nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, m_surface, &modeCount,
                                              modes.data());

    VkSurfaceFormatKHR surfaceFormat = ChooseFormat(formats);
    VkPresentModeKHR presentMode = ChoosePresentMode(modes, m_vsync);

    // Extent: honor the surface's fixed size, otherwise clamp the requested one.
    if (caps.currentExtent.width != std::numeric_limits<u32>::max()) {
        m_extent = caps.currentExtent;
    } else {
        m_extent.width = std::clamp(width, caps.minImageExtent.width,
                                    caps.maxImageExtent.width);
        m_extent.height = std::clamp(height, caps.minImageExtent.height,
                                     caps.maxImageExtent.height);
    }

    u32 imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = m_surface;
    info.minImageCount = imageCount;
    info.imageFormat = surfaceFormat.format;
    info.imageColorSpace = surfaceFormat.colorSpace;
    info.imageExtent = m_extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    const QueueFamilyIndices& q = m_device.Queues();
    u32 families[] = {*q.graphics, *q.present};
    if (*q.graphics != *q.present) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = families;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VK_CHECK(vkCreateSwapchainKHR(m_device.Logical(), &info, nullptr,
                                  &m_swapchain));
    m_format = surfaceFormat.format;

    u32 actualCount = 0;
    vkGetSwapchainImagesKHR(m_device.Logical(), m_swapchain, &actualCount,
                            nullptr);
    m_images.resize(actualCount);
    vkGetSwapchainImagesKHR(m_device.Logical(), m_swapchain, &actualCount,
                            m_images.data());

    m_imageViews.resize(actualCount);
    for (u32 i = 0; i < actualCount; ++i) {
        VkImageViewCreateInfo view{};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = m_images[i];
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = m_format;
        view.components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY};
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.baseMipLevel = 0;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.baseArrayLayer = 0;
        view.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(m_device.Logical(), &view, nullptr,
                                   &m_imageViews[i]));
    }

    LUMA_LOG_INFO("Vulkan", "swapchain {}x{} ({} images, {})", m_extent.width,
                  m_extent.height, actualCount,
                  presentMode == VK_PRESENT_MODE_FIFO_KHR ? "vsync" : "immediate");
}

void VulkanSwapchain::Destroy() {
    VkDevice device = m_device.Logical();
    for (VkImageView view : m_imageViews) {
        if (view) vkDestroyImageView(device, view, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();
    if (m_swapchain) {
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

}  // namespace Luma
