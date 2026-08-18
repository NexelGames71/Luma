#pragma once

#include <string>

#include "Luma/Math/Math.h"
#include "Luma/RHI/Renderer.h"
#include "Vulkan/VulkanCommon.h"
#include "Vulkan/VulkanMemory.h"

namespace Luma {

// Renders the procedural (physically based single-scattering) sky as a
// fullscreen background inside the scene view's dynamic-rendering pass.
// Self-contained: owns its own pipeline layout + pipeline, a params UBO +
// descriptor set, and the sky.vert/sky.frag shaders. The scene renderer just
// hands it the camera matrices and the SkyParams it received via the RHI, so the
// generic geometry passes stay unaware of "sky". The atmosphere shader module
// (sky_atmosphere.glsl) is shared with the deferred lighting pass so both
// paths produce the same sky.
class VulkanSkyPass {
public:
    VulkanSkyPass(VkPhysicalDevice physical, VkDevice device,
                  const std::string& shaderDir, VkSampleCountFlagBits samples,
                  VkFormat colorFormat, VkFormat depthFormat);
    ~VulkanSkyPass();

    VulkanSkyPass(const VulkanSkyPass&) = delete;
    VulkanSkyPass& operator=(const VulkanSkyPass&) = delete;

    // Records a fullscreen sky draw into an already-open render pass. `viewProj`
    // is the camera's view*projection; the camera position is extracted from the
    // inverse of `view`. Depth test/write are disabled so later geometry paints
    // over the sky.
    void Record(VkCommandBuffer cmd, const SkyParams& sky,
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
