#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "Luma/Core/Types.h"
#include "Luma/RHI/RHIResources.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/Renderer/SceneRenderer.h"

// GBuffer system for deferred shading. Inspired by UE5's GBuffer system
// but adapted for Luma's simpler architecture. Provides the geometry
// buffer layout and management for deferred rendering.

namespace Luma {
namespace Renderer2 {

using std::string;
using std::vector;
using namespace Luma::Math;

// ============================================================================
// GBuffer Layout
// ============================================================================

// GBuffer targets
enum class EGBufferTarget : u32 {
    Color,          // Albedo color (RGB) + Alpha (unused)
    Normal,         // World space normal (RGB) + Roughness (A)
    Material,       // Metallic (R) + Specular (G) + AO (B) + Unused (A)
    Depth,          // Depth buffer
    Count
};

// GBuffer texture formats
struct GBufferFormats {
    RHI::ETextureFormat colorFormat = RHI::ETextureFormat::R8G8B8A8_UNORM;
    RHI::ETextureFormat normalFormat = RHI::ETextureFormat::R16G16B16A16_FLOAT;
    RHI::ETextureFormat materialFormat = RHI::ETextureFormat::R8G8B8A8_UNORM;
    RHI::ETextureFormat depthFormat = RHI::ETextureFormat::D24_UNORM_S8_UINT;
};

// GBuffer description
struct GBufferDesc {
    u32 width = 1920;
    u32 height = 1080;
    GBufferFormats formats;
    bool enableHDR = false;
    u32 samples = 1;  // MSAA samples
};

// ============================================================================
// GBuffer
// ============================================================================

// GBuffer class for managing geometry buffer resources
class GBuffer {
public:
    GBuffer();
    ~GBuffer();
    
    // Get GBuffer description
    const GBufferDesc& GetDesc() const { return m_desc; }
    
    // Set GBuffer description
    void SetDesc(const GBufferDesc& desc) { m_desc = desc; }
    
    // Get GBuffer width
    u32 GetWidth() const { return m_desc.width; }
    
    // Get GBuffer height
    u32 GetHeight() const { return m_desc.height; }
    
    // Get color target
    RHI::RHITexture* GetColorTarget() const { return m_colorTarget; }
    
    // Get normal target
    RHI::RHITexture* GetNormalTarget() const { return m_normalTarget; }
    
    // Get material target
    RHI::RHITexture* GetMaterialTarget() const { return m_materialTarget; }
    
    // Get depth target
    RHI::RHITexture* GetDepthTarget() const { return m_depthTarget; }
    
    // Views
    RHI::RHIRenderTargetView* GetColorRTV() const { return m_colorRTV; }
    RHI::RHIRenderTargetView* GetNormalRTV() const { return m_normalRTV; }
    RHI::RHIRenderTargetView* GetMaterialRTV() const { return m_materialRTV; }
    RHI::RHIDepthStencilView* GetDepthDSV() const { return m_depthDSV; }

    RHI::RHIShaderResourceView* GetColorSRV() const { return m_colorSRV; }
    RHI::RHIShaderResourceView* GetNormalSRV() const { return m_normalSRV; }
    RHI::RHIShaderResourceView* GetMaterialSRV() const { return m_materialSRV; }
    RHI::RHIShaderResourceView* GetDepthSRV() const { return m_depthSRV; }

    // Create GBuffer resources
    bool Create(const GBufferDesc& desc, RHI::RHIDevice* device);
    
    // Resize GBuffer
    bool Resize(u32 width, u32 height, RHI::RHIDevice* device);
    
    // Destroy GBuffer resources
    void Destroy(RHI::RHIDevice* device);
    
    // Check if GBuffer is valid
    bool IsValid() const { return m_created; }
    
    // Clear GBuffer
    void Clear(RHI::RHICommandList* cmdList);
    
private:
    GBufferDesc m_desc;
    RHI::RHITexture* m_colorTarget = nullptr;
    RHI::RHITexture* m_normalTarget = nullptr;
    RHI::RHITexture* m_materialTarget = nullptr;
    RHI::RHITexture* m_depthTarget = nullptr;

    RHI::RHIRenderTargetView* m_colorRTV = nullptr;
    RHI::RHIRenderTargetView* m_normalRTV = nullptr;
    RHI::RHIRenderTargetView* m_materialRTV = nullptr;
    RHI::RHIDepthStencilView* m_depthDSV = nullptr;

    RHI::RHIShaderResourceView* m_colorSRV = nullptr;
    RHI::RHIShaderResourceView* m_normalSRV = nullptr;
    RHI::RHIShaderResourceView* m_materialSRV = nullptr;
    RHI::RHIShaderResourceView* m_depthSRV = nullptr;

    bool m_created = false;
};

// ============================================================================
// GBuffer Renderer
// ============================================================================

// GBuffer rendering pass
class GBufferRenderer {
public:
    GBufferRenderer();
    ~GBufferRenderer();
    
    // Get GBuffer
    GBuffer* GetGBuffer() const { return m_gBuffer; }
    
    // Set GBuffer
    void SetGBuffer(GBuffer* gbuffer) { m_gBuffer = gbuffer; }
    
    // Render geometry to GBuffer
    void RenderGeometry(RHI::RHICommandList* cmdList, const Luma::SceneView& sceneView);
    
    // Prepare GBuffer for rendering
    void Prepare(RHI::RHICommandList* cmdList);
    
    // Set render targets
    void SetRenderTargets(RHI::RHICommandList* cmdList);
    
    // Initialize with device
    bool Initialize(RHI::RHIDevice* device);
    void Shutdown();

private:
    GBuffer* m_gBuffer = nullptr;
    RHI::RHIDevice* m_device = nullptr;
    
    // Vulkan pipeline objects for rendering static meshes to G-Buffer
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_uboLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    
    // RHI Camera UBO
    RHI::RHIBuffer* m_cameraUBO = nullptr;
    bool m_initialized = false;
    
    bool InitPipeline();
    void CleanupPipeline();
    
    struct RhiMesh {
        RHI::RHIBuffer* vertexBuffer = nullptr;
        RHI::RHIBuffer* indexBuffer = nullptr;
        u32 indexCount = 0;
        u32 vertexCount = 0;
    };
    
    RhiMesh m_primitives[4];
    std::unordered_map<const Math::Vec3*, RhiMesh> m_customMeshes;
    
    void CreatePrimitives();
    void CleanupPrimitives();
    const RhiMesh* GetOrCreateCustomMesh(const Math::Vec3* vertices, u32 vertexCount, const u32* indices, u32 indexCount);

    // Pack normal and roughness
    Vec4 PackNormalRoughness(const Vec3& normal, f32 roughness);
    
    // Pack material attributes
    Vec4 PackMaterialMetallicAO(f32 metallic, f32 specular, f32 ao);
};

// ============================================================================
// GBuffer Pool
// ============================================================================

// Pool for managing multiple GBuffers (e.g., for different views)
class GBufferPool {
public:
    GBufferPool();
    ~GBufferPool();
    
    // Get GBuffer by index
    GBuffer* GetGBuffer(u32 index) const;
    
    // Get or create GBuffer
    GBuffer* GetOrCreateGBuffer(u32 width, u32 height, const GBufferFormats& formats, RHI::RHIDevice* device);
    
    // Resize all GBuffers
    void ResizeAll(u32 width, u32 height, RHI::RHIDevice* device);
    
    // Clear all GBuffers
    void ClearAll(RHI::RHICommandList* cmdList);
    
    // Destroy all GBuffers
    void DestroyAll(RHI::RHIDevice* device);
    
    // Get GBuffer count
    u32 GetGBufferCount() const { return static_cast<u32>(m_gbuffers.size()); }
    
private:
    vector<GBuffer*> m_gbuffers;
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Create GBuffer with default settings
inline GBuffer* CreateGBuffer(u32 width, u32 height, RHI::RHIDevice* device) {
    GBufferDesc desc;
    desc.width = width;
    desc.height = height;
    desc.enableHDR = false;
    desc.samples = 1;
    
    auto* gbuffer = new GBuffer();
    gbuffer->Create(desc, device);
    return gbuffer;
}

// Destroy GBuffer
inline void DestroyGBuffer(GBuffer* gbuffer, RHI::RHIDevice* device) {
    if (gbuffer) {
        gbuffer->Destroy(device);
        delete gbuffer;
    }
}

} // namespace Renderer2
} // namespace Luma