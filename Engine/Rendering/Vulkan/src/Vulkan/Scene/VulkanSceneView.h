#pragma once

#include <memory>

#include "Luma/RHI/Renderer.h"
#include "Vulkan/Grid/VulkanGridPass.h"
#include "Vulkan/Sky/VulkanSkyPass.h"
#include "Vulkan/UI/VulkanUIPass.h"
#include "Vulkan/VulkanCommon.h"
#include "Vulkan/VulkanMemory.h"

namespace Luma {

// Generic offscreen scene renderer: draws PBR mesh instances (built-in
// primitives) lit by a sun + sky IBL, plus a no-depth line channel for the
// gizmo overlay. Sky and grid are delegated to their own pass modules. This
// class renders primitives from SceneView data; it is feature-agnostic.
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
    void CreateLayouts();
    void CreateSceneUBO();
    void CreateShadowResources();
    VkPipeline CreateMeshPipeline(const std::string& shaderDir);
    VkPipeline CreateLinePipeline(const std::string& shaderDir, bool depthTest);
    VkPipeline CreateShadowPipeline(const std::string& shaderDir);
    void CreatePrimitives();
    void UploadLines(GpuBuffer& buffer, const LineVertex* lines, u32 count);

    VkPhysicalDevice m_physical;
    VkDevice m_device;
    VkQueue m_queue;
    VulkanUIPass& m_uiPass;

    VkCommandPool m_pool = VK_NULL_HANDLE;
    VkCommandBuffer m_cmd = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;

    // Lit mesh path: per-frame UBO (camera + lighting) + per-instance push.
    VkDescriptorSetLayout m_uboSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_uboPool = VK_NULL_HANDLE;
    VkDescriptorSet m_uboSet = VK_NULL_HANDLE;
    GpuBuffer m_ubo;
    VkPipelineLayout m_meshLayout = VK_NULL_HANDLE;
    VkPipeline m_meshPipeline = VK_NULL_HANDLE;

    // Built-in primitive meshes (indexed by MeshPrimitive).
    struct Primitive {
        GpuBuffer vertexBuffer;
        GpuBuffer indexBuffer;
        u32 indexCount = 0;
    };
    static constexpr u32 kPrimitiveCount = 4;
    Primitive m_primitives[kPrimitiveCount];

    // Line path (gizmo overlay): simple mvp+tint push, its own shader.
    VkPipelineLayout m_lineLayout = VK_NULL_HANDLE;
    VkPipeline m_linePipeline = VK_NULL_HANDLE;     // depth-tested lines
    VkPipeline m_overlayPipeline = VK_NULL_HANDLE;  // no-depth lines
    GpuBuffer m_lineBuffer;
    GpuBuffer m_overlayBuffer;

    // Cascaded sun shadow maps (directional): a depth texture array, one layer
    // per cascade, rendered depth-only from the light and sampled with PCSS.
    static constexpr u32 kShadowSize = 2048;
    static constexpr u32 kCascades = 4;
    static constexpr VkFormat kShadowFormat = VK_FORMAT_D32_SFLOAT;
    VkImage m_shadowImage = VK_NULL_HANDLE;
    VkDeviceMemory m_shadowMem = VK_NULL_HANDLE;
    VkImageView m_shadowArrayView = VK_NULL_HANDLE;          // 2D array (sampled)
    VkImageView m_shadowLayerViews[kCascades] = {};          // per-cascade (RT)
    VkSampler m_shadowSampler = VK_NULL_HANDLE;
    VkPipelineLayout m_shadowLayout = VK_NULL_HANDLE;
    VkPipeline m_shadowPipeline = VK_NULL_HANDLE;

    std::unique_ptr<VulkanSkyPass> m_skyPass;    // its own module (Sky/)
    std::unique_ptr<VulkanGridPass> m_gridPass;  // its own module (Grid/)

    // Offscreen targets (MSAA color+depth resolved to m_color for the UI).
    VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;
    VkImage m_color = VK_NULL_HANDLE;
    VkDeviceMemory m_colorMem = VK_NULL_HANDLE;
    VkImageView m_colorView = VK_NULL_HANDLE;
    VkImage m_msaaColor = VK_NULL_HANDLE;
    VkDeviceMemory m_msaaColorMem = VK_NULL_HANDLE;
    VkImageView m_msaaColorView = VK_NULL_HANDLE;
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
