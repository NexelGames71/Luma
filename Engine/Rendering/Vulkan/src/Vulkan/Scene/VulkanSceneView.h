#pragma once

#include "Luma/RHI/Renderer.h"
#include "Vulkan/UI/VulkanUIPass.h"
#include "Vulkan/VulkanCommon.h"
#include "Vulkan/VulkanMemory.h"

namespace Luma {

// Renders a demo 3D scene (rotating cube on a ground plane) into an offscreen
// color+depth target, then exposes the color image as a sampleable UI texture
// (registered with the UI pass) for display in the editor viewport.
class VulkanSceneView {
public:
    VulkanSceneView(VkPhysicalDevice physical, VkDevice device, VkQueue queue,
                    u32 graphicsFamily, VulkanUIPass& uiPass,
                    const std::string& shaderDir);
    ~VulkanSceneView();

    VulkanSceneView(const VulkanSceneView&) = delete;
    VulkanSceneView& operator=(const VulkanSceneView&) = delete;

    // Renders the scene at the given size; returns the UI texture handle to show.
    TextureHandle Render(u32 width, u32 height, const SceneView& scene);

private:
    void CreateTargets(u32 width, u32 height);
    void DestroyTargets();
    void CreatePipeline(const std::string& shaderDir);
    void CreateGeometry();

    VkPhysicalDevice m_physical;
    VkDevice m_device;
    VkQueue m_queue;
    VulkanUIPass& m_uiPass;

    VkCommandPool m_pool = VK_NULL_HANDLE;
    VkCommandBuffer m_cmd = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    GpuBuffer m_vertexBuffer;
    GpuBuffer m_indexBuffer;

    // Offscreen targets.
    VkImage m_color = VK_NULL_HANDLE;
    VkDeviceMemory m_colorMem = VK_NULL_HANDLE;
    VkImageView m_colorView = VK_NULL_HANDLE;
    VkImage m_depth = VK_NULL_HANDLE;
    VkDeviceMemory m_depthMem = VK_NULL_HANDLE;
    VkImageView m_depthView = VK_NULL_HANDLE;
    u32 m_width = 0;
    u32 m_height = 0;

    TextureHandle m_textureHandle = 0;

    static constexpr VkFormat kColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    static constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;
};

}  // namespace Luma
