#pragma once

#include <string>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"
#include "Luma/RHI/RHIResources.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/Renderer/GBufferInfo.h"

// Lighting system for deferred shading. Inspired by UE5's lighting system
// but adapted for Luma's simpler architecture. Provides light management
// and deferred lighting pass implementation.

namespace Luma {
namespace Renderer2 {

using std::string;
using std::vector;
using namespace Luma::Math;

// Forward declarations
class GBuffer;
class LightAccumulationBuffer;

// ============================================================================
// Light Types
// ============================================================================

// Light type (using existing LightSceneInfo types for consistency)
enum class ELightType : u32 {
    Directional = 0,
    Point = 1,
    Spot = 2,
};

// Light data structure (compatible with existing RHI SceneLight)
struct LightData {
    Vec3 position{0.0f, 3.0f, 0.0f};
    u32 type = static_cast<u32>(ELightType::Point);
    Vec3 direction{0.0f, -1.0f, 0.0f};
    f32 range = 12.0f;
    Vec3 color{1.0f, 1.0f, 1.0f};
    f32 intensity = 6.0f;
    f32 cosInner = 0.94f;
    f32 cosOuter = 0.87f;
    
    // Convert from SceneLight
    static LightData FromSceneLight(const Luma::SceneLight& sceneLight) {
        LightData data;
        data.position = sceneLight.position;
        data.type = sceneLight.type;
        data.direction = sceneLight.direction;
        data.range = sceneLight.range;
        data.color = sceneLight.color;
        data.intensity = sceneLight.intensity;
        data.cosInner = sceneLight.cosInner;
        data.cosOuter = sceneLight.cosOuter;
        return data;
    }
};

// ============================================================================
// Lighting Parameters
// ============================================================================

// Lighting parameters (compatible with existing RHI LightingParams)
struct LightingParameters {
    Vec3 sunDirection{0.35f, 0.65f, 0.55f};
    Vec3 sunColor{1.0f, 0.96f, 0.9f};
    f32 sunIntensity = 3.0f;
    Vec3 skyZenith{0.20f, 0.34f, 0.62f};
    Vec3 skyHorizon{0.62f, 0.68f, 0.78f};
    Vec3 groundColor{0.16f, 0.16f, 0.17f};
    f32 iblIntensity = 1.0f;
    bool sunShadows = true;
    f32 shadowDistance = 80.0f;
    f32 shadowSoftness = 1.6f;
    f32 shadowBias = 0.0015f;
    f32 normalBias = 0.02f;
    f32 cascadeSplitLambda = 0.6f;
    u32 cascadeCount = 4;
    
    // Convert from LightingParams
    static LightingParameters FromLightingParams(const Luma::LightingParams& params) {
        LightingParameters data;
        data.sunDirection = params.sunDirection;
        data.sunColor = params.sunColor;
        data.sunIntensity = params.sunIntensity;
        data.skyZenith = params.skyZenith;
        data.skyHorizon = params.skyHorizon;
        data.groundColor = params.groundColor;
        data.iblIntensity = params.iblIntensity;
        data.sunShadows = params.sunShadows;
        data.shadowDistance = params.shadowDistance;
        data.shadowSoftness = params.shadowSoftness;
        data.shadowBias = params.shadowBias;
        data.normalBias = params.normalBias;
        data.cascadeSplitLambda = params.cascadeSplitLambda;
        data.cascadeCount = params.cascadeCount;
        return data;
    }
};

// ============================================================================
// Lighting Renderer
// ============================================================================

// Lighting renderer for deferred shading
class LightingRenderer {
public:
    LightingRenderer();
    ~LightingRenderer();
    
    // Get GBuffer
    GBuffer* GetGBuffer() const { return m_gBuffer; }
    
    // Set GBuffer
    void SetGBuffer(GBuffer* gbuffer) { m_gBuffer = gbuffer; }
    
    // Set lighting parameters
    void SetLightingParameters(const LightingParameters& params) { m_lightingParams = params; }
    
    // Get lighting parameters
    const LightingParameters& GetLightingParameters() const { return m_lightingParams; }
    
    // Add light
    void AddLight(const LightData& light);
    
    // Remove light
    void RemoveLight(u32 index);
    
    // Get light count
    u32 GetLightCount() const { return static_cast<u32>(m_lights.size()); }
    
    // Clear lights
    void ClearLights();
    
    // Render lighting pass
    void RenderLighting(RHI::RHICommandList* cmdList);
    
    // Render directional light with shadows
    void RenderDirectionalLight(RHI::RHICommandList* cmdList);
    
    // Render punctual lights
    void RenderPunctualLights(RHI::RHICommandList* cmdList);
    
    // Render IBL (image-based lighting)
    void RenderIBL(RHI::RHICommandList* cmdList);
    
    // Prepare lighting pass
    void Prepare(RHI::RHICommandList* cmdList);
    
    // Initialize/Shutdown
    bool Initialize(RHI::RHIDevice* device, LightAccumulationBuffer* accumBuffer);
    void Shutdown();

private:
    GBuffer* m_gBuffer = nullptr;
    LightAccumulationBuffer* m_accumBuffer = nullptr;
    RHI::RHIDevice* m_device = nullptr;
    LightingParameters m_lightingParams;
    vector<LightData> m_lights;
    
    // Vulkan deferred lighting pipeline
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkSampler m_gbufferSampler = VK_NULL_HANDLE;
    
    // Uniform Buffer for deferred lighting parameters
    RHI::RHIBuffer* m_lightingUBO = nullptr;
    bool m_initialized = false;
    
    bool InitPipeline();
    void CleanupPipeline();
    void UpdateDescriptorSet();
    
    // Set lighting uniforms
    void SetLightingUniforms(RHI::RHICommandList* cmdList);
};

// ============================================================================
// Light Accumulation Buffer
// ============================================================================

// Light accumulation buffer for combining multiple light passes
class LightAccumulationBuffer {
public:
    LightAccumulationBuffer();
    ~LightAccumulationBuffer();
    
    // Get light accumulation texture
    RHI::RHITexture* GetAccumulationTexture() const { return m_accumulationTexture; }
    RHI::RHIRenderTargetView* GetAccumulationRTV() const { return m_accumulationRTV; }
    RHI::RHIShaderResourceView* GetAccumulationSRV() const { return m_accumulationSRV; }
    
    // Get the underlying Vulkan image view (for registration with UI system)
    VkImageView GetVulkanImageView() const;
    
    // Create accumulation buffer
    bool Create(u32 width, u32 height, RHI::ETextureFormat format = RHI::ETextureFormat::R16G16B16A16_FLOAT, RHI::RHIDevice* device = nullptr);
    
    // Resize accumulation buffer
    bool Resize(u32 width, u32 height, RHI::RHIDevice* device = nullptr);
    
    // Destroy accumulation buffer
    void Destroy();
    
    // Clear accumulation buffer
    void Clear(RHI::RHICommandList* cmdList);
    
    // Check if valid
    bool IsValid() const { return m_created; }
    
private:
    RHI::RHITexture* m_accumulationTexture = nullptr;
    RHI::RHIRenderTargetView* m_accumulationRTV = nullptr;
    RHI::RHIShaderResourceView* m_accumulationSRV = nullptr;
    RHI::RHIDevice* m_device = nullptr;
    u32 m_width = 0;
    u32 m_height = 0;
    bool m_created = false;
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Create lighting renderer
inline LightingRenderer* CreateLightingRenderer() {
    return new LightingRenderer();
}

// Destroy lighting renderer
inline void DestroyLightingRenderer(LightingRenderer* renderer) {
    if (renderer) {
        delete renderer;
    }
}

} // namespace Renderer2
} // namespace Luma