#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "Luma/RHI/Renderer.h"
#include "Vulkan/VulkanCommon.h"
#include "Vulkan/VulkanMemory.h"
#include "Vulkan/VulkanTexture.h"

namespace Luma {

// Renders Luma Slate's backend-agnostic UIDrawData: a textured, alpha-blended
// 2D pipeline plus per-frame dynamic vertex/index buffers and a texture table.
class VulkanUIPass {
public:
    VulkanUIPass(VkPhysicalDevice physical, VkDevice device,
                 VkCommandPool uploadPool, VkQueue uploadQueue,
                 VkFormat colorFormat, u32 framesInFlight,
                 const std::string& shaderDir);
    ~VulkanUIPass();

    VulkanUIPass(const VulkanUIPass&) = delete;
    VulkanUIPass& operator=(const VulkanUIPass&) = delete;

    TextureHandle CreateTexture(u32 width, u32 height, const void* rgba8Pixels);
    void DestroyTexture(TextureHandle handle);
    TextureHandle WhiteTexture() const { return m_whiteTexture; }

    // Records draw commands into `cmd` for the given frame slot.
    void Record(VkCommandBuffer cmd, u32 frame, const UIDrawData& data);

private:
    void EnsureBuffer(GpuBuffer& buffer, VkDeviceSize needed,
                      VkBufferUsageFlags usage);

    VkPhysicalDevice m_physical;
    VkDevice m_device;
    VkCommandPool m_uploadPool;
    VkQueue m_uploadQueue;

    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    std::vector<GpuBuffer> m_vertexBuffers;  // per frame in flight
    std::vector<GpuBuffer> m_indexBuffers;

    std::unordered_map<TextureHandle, std::unique_ptr<VulkanTexture>> m_textures;
    TextureHandle m_nextHandle = 1;
    TextureHandle m_whiteTexture = 0;
};

}  // namespace Luma
