#pragma once

#include <string>

#include "Luma/Math/Math.h"
#include "Luma/RHI/Renderer.h"
#include "Vulkan/VulkanCommon.h"
#include "Vulkan/VulkanMemory.h"

namespace Luma {

// Renders the infinite editor ground grid as an analytic fullscreen plane pass
// (grid.vert/grid.frag). Its own module: owns pipeline, layout, a params UBO and
// its descriptor set. The scene renderer just hands it the camera matrices and
// the GridParams received via the RHI.
class VulkanGridPass {
public:
    VulkanGridPass(VkPhysicalDevice physical, VkDevice device,
                   const std::string& shaderDir, VkSampleCountFlagBits samples,
                   VkFormat colorFormat, VkFormat depthFormat);
    ~VulkanGridPass();

    VulkanGridPass(const VulkanGridPass&) = delete;
    VulkanGridPass& operator=(const VulkanGridPass&) = delete;

    // Records the grid into an already-open render pass. Depth-tested + written
    // so scene geometry occludes it; alpha-blended over the sky.
    void Record(VkCommandBuffer cmd, const GridParams& grid,
                const Math::Mat4& view, const Math::Mat4& viewProj);

private:
    VkDevice m_device;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
    GpuBuffer m_ubo;
};

}  // namespace Luma
