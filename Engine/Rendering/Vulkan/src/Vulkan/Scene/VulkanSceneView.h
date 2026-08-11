#pragma once

#include "Luma/RHI/Renderer.h"
#include "Vulkan/UI/VulkanUIPass.h"
#include "Vulkan/VulkanCommon.h"
#include "Vulkan/VulkanMemory.h"

namespace Luma {

// Generic offscreen scene renderer: draws cube instances (triangles), plus two
// line channels (depth-tested and overlay) that feature modules (grid, gizmo)
// fill via SceneView. The color target is exposed as a UI-sampleable texture.
// This class is feature-agnostic: it renders primitives, not "grids" or
// "gizmos".
class VulkanSceneView {
public:
    VulkanSceneView(VkPhysicalDevice physical, VkDevice device, VkQueue queue,
                    u32 graphicsFamily, VulkanUIPass& uiPass,
                    const std::string& shaderDir);
    ~VulkanSceneView();

    VulkanSceneView(const VulkanSceneView&) = delete;
    VulkanSceneView& operator=(const VulkanSceneView&) = delete;

    TextureHandle Render(u32 width, u32 height, const SceneView& scene);

private:
    void CreateTargets(u32 width, u32 height);
    void DestroyTargets();
    void CreatePipelineLayout();
    VkPipeline CreatePipeline(const std::string& shaderDir,
                              VkPrimitiveTopology topology, bool depthTest,
                              bool depthWrite);
    void CreateGeometry();
    void UploadLines(GpuBuffer& buffer, const LineVertex* lines, u32 count);

    VkPhysicalDevice m_physical;
    VkDevice m_device;
    VkQueue m_queue;
    VulkanUIPass& m_uiPass;

    VkCommandPool m_pool = VK_NULL_HANDLE;
    VkCommandBuffer m_cmd = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_trianglePipeline = VK_NULL_HANDLE;
    VkPipeline m_linePipeline = VK_NULL_HANDLE;      // depth-tested lines (grid)
    VkPipeline m_overlayPipeline = VK_NULL_HANDLE;   // no-depth lines (gizmo)

    GpuBuffer m_vertexBuffer;  // cube mesh
    GpuBuffer m_indexBuffer;
    GpuBuffer m_lineBuffer;     // dynamic, per-frame
    GpuBuffer m_overlayBuffer;  // dynamic, per-frame

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
