#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Luma/Math/Math.h"
#include "Luma/Renderer/DeferredShadingRenderer.h"
#include "Vulkan/Sky/AtmosphereParams.h"
#include "Vulkan/VulkanMemory.h"

namespace Luma {
class VulkanUIPass;
class VulkanGridPass;
namespace Rendering {

/**
 * Vulkan deferred renderer for the editor viewport (and asset thumbnails).
 *
 * Pipeline (all offscreen, into the light-accumulation buffer):
 *   1. G-Buffer pass  - renders scene instances into MRT attachments
 *      (albedo R8G8B8A8_UNORM, world-normal+roughness R16G16B16A16_SFLOAT,
 *      metallic/specular/AO R8G8B8A8_UNORM) plus a D32 depth attachment.
 *   2. Lighting pass  - fullscreen triangle (deferred_lighting shaders)
 *      reads the G-Buffer + lights UBO and writes the lit, tone-mapped,
 *      gamma-encoded color into the light-accumulation buffer.
 *   3. Grid pass      - the editor ground grid, depth-tested against the
 *      G-Buffer depth so scene geometry occludes it (optional; skipped when
 *      the scene's grid is disabled).
 *
 * The light-accumulation image is registered with the UI pass as an external
 * texture so the viewport panel can display it (see GetViewportTextureHandle).
 */
class VulkanDeferredRenderer {
public:
    struct Attachment {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
    };

    struct GBuffer {
        int32_t width = 0;
        int32_t height = 0;
        Attachment albedo;     // R8G8B8A8_UNORM
        Attachment normal;     // R16G16B16A16_SFLOAT (+ roughness in alpha)
        Attachment material;   // R8G8B8A8_UNORM (metallic/specular/AO)
        Attachment depth;      // D32_SFLOAT
        Attachment lightAccum; // final lit result shown in the viewport
        VkSampler sampler = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkRenderPass lightingRenderPass = VK_NULL_HANDLE;
        VkFramebuffer lightingFramebuffer = VK_NULL_HANDLE;
    };

    VulkanDeferredRenderer(VkPhysicalDevice physicalDevice, VkDevice device,
                           VkQueue graphicsQueue, uint32_t graphicsQueueFamilyIndex,
                           VulkanUIPass& uiPass, const std::string& shaderDir);
    ~VulkanDeferredRenderer();

    VulkanDeferredRenderer(const VulkanDeferredRenderer&) = delete;
    VulkanDeferredRenderer& operator=(const VulkanDeferredRenderer&) = delete;

    // Initialization
    bool Initialize(int32_t width, int32_t height);
    void Cleanup();

    // Public API for editor integration
    void SetViewportDimensions(int32_t width, int32_t height);
    void PrepareScene();
    void RenderScene(const Renderer2::DeferredSceneView& scene);
    const Attachment* GetLightAccumulationBuffer() const;

    // UI texture handle for the light-accumulation buffer (0 until first render).
    // Owned by this renderer; stays valid across resizes.
    uint64_t GetViewportTextureHandle() const { return m_textureHandle; }

    // --- Material texture cache -------------------------------------------------
    // Uploads an RGBA8 texture and returns a stable index for SceneInstance's
    // baseColorTex/normalTex/roughnessTex/metallicTex (-1 = none). The GBuffer
    // samples the map only when the instance's index >= 0, so materials with no
    // map assigned keep using their constant values. The index is stable for the
    // lifetime of this renderer; textures are never evicted.
    //
    // `key` dedupes: uploading the same key again returns the existing index
    // (the pixels are ignored). Returns -1 on failure.
    i32 UploadMaterialTexture(const std::string& key, u32 width, u32 height,
                              const void* rgba8Pixels);

    // Increments every time the texture cache is rebuilt (Cleanup+Initialize,
    // e.g. on viewport resize). Callers that cache texture indices can compare
    // this value against the generation they uploaded at and re-upload when it
    // changes, since stale indices point at destroyed/recreated GPU resources.
    u32 GetMaterialTextureGeneration() const { return m_materialTextureGeneration; }

    // True once Initialize() succeeded (resources exist: command pool, GBuffer
    // descriptor set, samplers). UploadMaterialTexture is only valid then.
    bool IsInitialized() const { return m_initialized; }

private:
    // Per-instance material push (matches gbuffer.vert push constant block).
    struct MeshPush {
        Math::Mat4 model;
        f32 albedo[4];    // rgb, w = metallic
        f32 material[4];  // x = roughness
        i32 texIdx[4];    // baseColor, normal, roughness, metallic (-1 = none)
    };

    // Camera UBO (matches CameraUBO in gbuffer.vert).
    struct GbufferUBO {
        Math::Mat4 viewProj;
        f32 camPos[4];
    };

    // One GPU light (matches `Light` in deferred_lighting.frag).
    struct GpuLight {
        f32 posType[4];   // xyz position, w type (0 dir, 1 point, 2 spot)
        f32 dirRange[4];  // xyz direction, w range
        f32 color[4];     // rgb color, w intensity
        f32 spot[4];      // x cosInner, y cosOuter
    };
    static constexpr u32 kMaxLights = 16;
    static constexpr u32 kCascadesUBO = 4;
    static constexpr u32 kShadowSize = 2048;
    static constexpr u32 kCascades = 4;
    static constexpr VkFormat kShadowFormat = VK_FORMAT_D32_SFLOAT;

    // Lighting UBO (matches LightingUBO in deferred_lighting.frag).
    struct LightingUBO {
        Math::Mat4 invViewProj;
        f32 camPos[4];
        f32 camForward[4];   // xyz camera forward (for cascade selection)
        f32 sunDir[4];       // xyz direction TO sun, w = below-horizon sky fade
        f32 sunColor[4];     // rgb, w intensity
        f32 ambientColor[4]; // rgb, w intensity
        f32 params[4];       // x = lightCount, y = sunShadows, z = 1/shadowSize
        f32 shadowParams[4]; // x = softness, y = cascadeCount, z = shadowBias
        f32 cascadeSplits[4];
        Math::Mat4 cascadeViewProj[kCascadesUBO];
        Rendering::AtmosphereParams atmo;  // physical sky params (sky_atmosphere.glsl)
        GpuLight lights[kMaxLights];
    };

    // Resource creation
    bool CreateCommandPool();
    bool CreateGBuffer(int32_t width, int32_t height);
    bool CreateAttachment(VkFormat format, VkImageUsageFlags usage,
                          Attachment* attachment);
    void DestroyAttachment(Attachment& attachment);
    bool CreateRenderPass();
    bool CreateLightingRenderPass();
    bool CreateFramebuffer();
    bool CreateLightingFramebuffer();
    bool CreateSampler();
    bool CreateUBOs();
    bool CreateLayouts();
    bool CreateDescriptorSets();
    bool CreatePipelines(const std::string& shaderDir);
    void CreatePrimitives();
    void CleanupPrimitives();

    // Cascaded shadow maps (sun / directional light)
    bool CreateShadowResources();
    void DestroyShadowResources();
    bool CreateShadowPipeline(const std::string& shaderDir);

    // Custom mesh support (same contract as VulkanSceneView). `normals` and
    // `uvs` are optional (null → defaults); both are `vertexCount` long when set.
    struct CustomMesh {
        GpuBuffer vertexBuffer;
        GpuBuffer indexBuffer;
        u32 vertexCount = 0;
        u32 indexCount = 0;
        bool valid = false;
    };
    const CustomMesh* GetOrCreateCustomMesh(const Math::Vec3* vertices,
                                            u32 vertexCount,
                                            const u32* indices,
                                            u32 indexCount,
                                            const Math::Vec3* normals = nullptr,
                                            const Math::Vec2* uvs = nullptr,
                                            const Math::Vec3* tangents = nullptr);
    void CleanupCustomMeshes();

    // Material texture cache entries: one GPU texture per slot.
    struct MaterialTexture {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
    };
    static constexpr u32 kMaxMaterialTextures = 32;
    std::vector<MaterialTexture> m_materialTextures;
    std::unordered_map<std::string, i32> m_materialTextureIndex;
    VkSampler m_materialSampler = VK_NULL_HANDLE;
    u32 m_materialTextureGeneration = 0;  // bumped in Initialize (cache rebuild)

    // Vulkan handles
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamilyIndex = 0;
    VulkanUIPass& m_uiPass;
    std::string m_shaderDir;
    bool m_initialized = false;
    bool m_gBufferInitialized = false;

    // GBuffer (MRT attachments + light accumulation)
    GBuffer m_gBuffer;

    // Pipelines
    VkPipelineLayout m_gbufferLayout = VK_NULL_HANDLE;
    VkPipeline m_gbufferPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_lightingLayout = VK_NULL_HANDLE;
    VkPipeline m_lightingPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_shadowLayout = VK_NULL_HANDLE;
    VkPipeline m_shadowPipeline = VK_NULL_HANDLE;

    // Descriptor resources
    VkDescriptorSetLayout m_gbufferSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_gbufferPool = VK_NULL_HANDLE;
    VkDescriptorSet m_gbufferSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_lightingSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_lightingPool = VK_NULL_HANDLE;
    VkDescriptorSet m_lightingSet = VK_NULL_HANDLE;

    // Uniform buffers
    GpuBuffer m_gbufferUBO;
    GpuBuffer m_lightingUBO;

    // Command resources
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;

    // Built-in primitive meshes (indexed by MeshPrimitive)
    struct Primitive {
        GpuBuffer vertexBuffer;
        GpuBuffer indexBuffer;
        u32 indexCount = 0;
    };
    static constexpr u32 kPrimitiveCount = 4;
    Primitive m_primitives[kPrimitiveCount];

    // Custom mesh cache (keyed by the scene instance's vertex pointer)
    std::unordered_map<const Math::Vec3*, CustomMesh> m_customMeshes;

    // Cascaded shadow map resources (sun / directional light)
    VkImage m_shadowImage = VK_NULL_HANDLE;
    VkDeviceMemory m_shadowMem = VK_NULL_HANDLE;
    VkImageView m_shadowArrayView = VK_NULL_HANDLE;
    VkImageView m_shadowLayerViews[kCascades] = {};
    VkSampler m_shadowSampler = VK_NULL_HANDLE;
    bool m_shadowImageInitialized = false;

    // Editor ground grid (created with 1-sample targets matching this pass)
    std::unique_ptr<VulkanGridPass> m_gridPass;

    // UI texture handle registered for the light-accumulation buffer
    uint64_t m_textureHandle = 0;

    // Image view the UI texture currently points at (re-pointed on resize)
    VkImageView m_registeredLightView = VK_NULL_HANDLE;

    static constexpr VkFormat kAlbedoFormat = VK_FORMAT_R8G8B8A8_UNORM;
    static constexpr VkFormat kNormalFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    static constexpr VkFormat kMaterialFormat = VK_FORMAT_R8G8B8A8_UNORM;
    static constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;
    static constexpr VkFormat kLightFormat = VK_FORMAT_R8G8B8A8_UNORM;
};

} // namespace Rendering
} // namespace Luma
