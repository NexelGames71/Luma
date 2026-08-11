#pragma once

#include <string>

#include "Luma/Math/Math.h"
#include "Luma/RHI/Renderer.h"
#include "Vulkan/VulkanCommon.h"

namespace Luma {

// Renders the procedural (Preetham) sky as a fullscreen background inside the
// scene view's dynamic-rendering pass. Self-contained: owns its own pipeline
// layout + pipeline and the sky.vert/sky.frag shaders. The scene renderer just
// hands it the camera matrices and the SkyParams it received via the RHI, so the
// generic geometry passes stay unaware of "sky".
class VulkanSkyPass {
public:
    VulkanSkyPass(VkDevice device, const std::string& shaderDir,
                  VkSampleCountFlagBits samples, VkFormat colorFormat,
                  VkFormat depthFormat);
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
};

}  // namespace Luma
