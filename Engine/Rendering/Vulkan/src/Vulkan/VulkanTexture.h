#pragma once

#include "Vulkan/VulkanCommon.h"

namespace Luma {

// An RGBA8 sampled texture with its own descriptor set (combined image sampler)
// allocated from a shared pool/layout and using a shared sampler.
class VulkanTexture {
public:
    VulkanTexture(VkPhysicalDevice physical, VkDevice device,
                  VkCommandPool uploadPool, VkQueue uploadQueue, u32 width,
                  u32 height, const void* rgba8Pixels, VkDescriptorPool pool,
                  VkDescriptorSetLayout layout, VkSampler sampler);
    ~VulkanTexture();

    VulkanTexture(const VulkanTexture&) = delete;
    VulkanTexture& operator=(const VulkanTexture&) = delete;

    VkDescriptorSet DescriptorSet() const { return m_set; }

private:
    VkDevice m_device;
    VkDescriptorPool m_pool;
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
};

}  // namespace Luma
