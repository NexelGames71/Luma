#pragma once

#include <string>

#include "Vulkan/VulkanCommon.h"

namespace Luma {

// A graphics pipeline for dynamic rendering. Viewport and scissor are dynamic so
// the pipeline survives swapchain resizes. Milestone 3-A: no vertex input, no
// descriptors (the triangle is generated in the vertex shader).
class VulkanPipeline {
public:
    VulkanPipeline(VkDevice device, VkFormat colorFormat,
                   const std::string& vertSpirv, const std::string& fragSpirv);
    ~VulkanPipeline();

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    VkPipeline Handle() const { return m_pipeline; }
    VkPipelineLayout Layout() const { return m_layout; }

private:
    VkDevice m_device;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace Luma
