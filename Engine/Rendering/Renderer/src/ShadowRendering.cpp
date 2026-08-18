#include "Luma/Renderer/Shadows.h"

#include <cmath>
#include <algorithm>
#include "Luma/RHI/RHIContext.h"

namespace Luma {
namespace Renderer2 {

using std::vector;

// ============================================================================
// Shadow Map
// ============================================================================

ShadowMap::ShadowMap()
    : m_created(false) {
}

ShadowMap::~ShadowMap() {
    Destroy();
}

bool ShadowMap::Create(const ShadowMapDesc& desc, RHI::RHIDevice* device) {
    m_desc = desc;
    m_device = device;
    
    if (!device) {
        m_created = true;
        return true;
    }

    auto* factory = device->GetContext()->GetResourceFactory();
    if (!factory) {
        return false;
    }
    
    auto makeTexture = [&](RHI::ETextureFormat format) -> RHI::RHITexture* {
        RHI::TextureDesc td;
        td.width = m_desc.width;
        td.height = m_desc.height;
        td.mipLevels = 1;
        td.arraySize = 1;
        td.format = format;
        td.usage = RHI::ETextureUsage::DepthStencil | RHI::ETextureUsage::ShaderResource;
        td.flags = RHI::ETextureFlags::DepthStencilTargetable | RHI::ETextureFlags::ShaderResource;
        return factory->CreateTexture(td);
    };

    if (desc.type == EShadowMapType::Cascaded) {
        m_cascadeTextures.resize(desc.cascadeCount);
        for (u32 i = 0; i < desc.cascadeCount; ++i) {
            m_cascadeTextures[i] = makeTexture(desc.format);
            if (!m_cascadeTextures[i]) return false;
        }
    } else {
        m_shadowMapTexture = makeTexture(desc.format);
        if (!m_shadowMapTexture) return false;
    }
    
    m_created = true;
    return true;
}

bool ShadowMap::Resize(u32 width, u32 height, RHI::RHIDevice* device) {
    Destroy();
    ShadowMapDesc next = m_desc;
    next.width = width;
    next.height = height;
    return Create(next, device);
}

void ShadowMap::Destroy() {
    if (m_device) {
        auto* factory = m_device->GetContext()->GetResourceFactory();
        if (factory) {
            if (m_shadowMapTexture) {
                factory->DestroyTexture(m_shadowMapTexture);
            }
            for (auto* tex : m_cascadeTextures) {
                if (tex) {
                    factory->DestroyTexture(tex);
                }
            }
        }
    }
    m_shadowMapTexture = nullptr;
    m_cascadeTextures.clear();
    m_device = nullptr;
    m_created = false;
}

// ============================================================================
// Shadow Renderer
// ============================================================================

ShadowRenderer::ShadowRenderer()
    : m_shadowMap(nullptr) {
}

ShadowRenderer::~ShadowRenderer() {
}

void ShadowRenderer::RenderShadowMap(RHI::RHICommandList* cmdList, const LightData& light, const Luma::SceneView& sceneView) {
    if (!m_shadowMap || !m_shadowMap->IsValid()) {
        return;
    }
    
    // TODO: Render shadow map for a light
    // This would:
    // 1. Set shadow map as render target
    // 2. Configure viewport for shadow map resolution
    // 3. Set light-specific view/projection matrices
    // 4. Render scene geometry from light's perspective
    // 5. Store depth information for shadow comparison
    
    (void)cmdList;
    (void)light;
    (void)sceneView;
}

void ShadowRenderer::RenderCascadedShadowMaps(RHI::RHICommandList* cmdList, const LightData& light, const Luma::SceneView& sceneView) {
    if (!m_shadowMap || !m_shadowMap->IsValid()) {
        return;
    }
    
    // TODO: Render cascaded shadow maps for directional light
    // This would:
    // 1. Calculate cascade split distances
    // 2. For each cascade:
    //    a. Set appropriate cascade render target
    //    b. Calculate cascade view-projection matrix
    //    c. Render scene geometry for that cascade
    //    d. Store depth information
    // 3. Store cascade matrices for lighting pass
    
    (void)cmdList;
    (void)light;
    (void)sceneView;
}

void ShadowRenderer::RenderCubeShadowMap(RHI::RHICommandList* cmdList, const LightData& light, const Luma::SceneView& sceneView) {
    if (!m_shadowMap || !m_shadowMap->IsValid()) {
        return;
    }
    
    // TODO: Render cube shadow map for point light
    // This would:
    // 1. Set cube map render target
    // 2. For each cube face:
    //    a. Set appropriate face viewport
    //    b. Calculate face view matrix
    //    c. Render scene geometry from that direction
    //    d. Store depth information
    // 3. Bind cube map for lighting pass
    
    (void)cmdList;
    (void)light;
    (void)sceneView;
}

void ShadowRenderer::Prepare(RHI::RHICommandList* cmdList) {
    // TODO: Prepare shadow rendering
    // This would:
    // 1. Clear shadow map textures
    // 2. Set up shadow render targets
    // 3. Configure viewport for shadow rendering
    
    (void)cmdList;
}

vector<f32> ShadowRenderer::CalculateCascadeSplits(f32 nearPlane, f32 farPlane, f32 splitLambda, u32 cascadeCount) {
    vector<f32> splits;
    
    // Calculate cascade split distances using practical split scheme
    // Based on logarithmic distribution
    (void)splitLambda; // Used for future split calculation refinement
    for (u32 i = 0; i <= cascadeCount; ++i) {
        f32 i_float = static_cast<f32>(i);
        f32 n_float = static_cast<f32>(cascadeCount);
        
        // Practical split scheme
        f32 logNear = std::log(nearPlane);
        f32 logFar = std::log(farPlane);
        f32 logRange = logFar - logNear;
        
        f32 depth = std::exp(logNear + (i_float / n_float) * logRange);
        splits.push_back(depth);
    }
    
    return splits;
}

Mat4 ShadowRenderer::CalculateLightViewMatrix(const LightData& light) {
    // Calculate view matrix for light
    Vec3 lightPos = light.position;
    Vec3 lightDir = light.direction;
    Vec3 lightTarget = lightPos + lightDir;
    Vec3 lightUp = Vec3(0.0f, 1.0f, 0.0f);
    
    return LookAt(lightPos, lightTarget, lightUp);
}

Mat4 ShadowRenderer::CalculateLightProjectionMatrix(const LightData& light) {
    // Calculate projection matrix for light
    // Directional lights use orthographic projection
    // Point/spot lights use perspective projection
    
    if (static_cast<ELightType>(light.type) == ELightType::Directional) {
        // Orthographic projection for directional light
        f32 orthoSize = light.range * 2.0f;
        return Ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, light.range);
    } else {
        // Perspective projection for point/spot lights
        f32 aspect = 1.0f;
        f32 fov = Radians(90.0f);
        return Perspective(fov, aspect, 0.1f, light.range);
    }
}

Mat4 ShadowRenderer::CalculateCascadeViewProjection(const Vec3& lightPos, const Vec3& lightDir, f32 nearPlane, f32 farPlane, const Luma::SceneView& sceneView) {
    // Calculate cascade view-projection matrix
    // This would be different for each cascade
    (void)sceneView; // Used for future cascade-specific camera view calculations
    Mat4 lightView = LookAt(lightPos, lightPos + lightDir, Vec3(0.0f, 1.0f, 0.0f));
    
    f32 orthoSize = farPlane * 0.5f;
    Mat4 lightProj = Ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);
    
    return lightProj * lightView;
}

// ============================================================================
// Shadow Manager
// ============================================================================

ShadowManager& ShadowManager::GetInstance() {
    static ShadowManager instance;
    return instance;
}

ShadowManager::~ShadowManager() {
    Clear();
}

ShadowMap* ShadowManager::CreateShadowMap(const ShadowMapDesc& desc, RHI::RHIDevice* device) {
    auto* shadowMap = new ShadowMap();
    shadowMap->Create(desc, device);
    m_shadowMaps.push_back(shadowMap);
    return shadowMap;
}

void ShadowManager::DestroyShadowMap(ShadowMap* shadowMap) {
    auto it = std::find(m_shadowMaps.begin(), m_shadowMaps.end(), shadowMap);
    if (it != m_shadowMaps.end()) {
        m_shadowMaps.erase(it);
        shadowMap->Destroy();
        delete shadowMap;
    }
}

void ShadowManager::Clear() {
    for (auto* shadowMap : m_shadowMaps) {
        shadowMap->Destroy();
        delete shadowMap;
    }
    m_shadowMaps.clear();
}

} // namespace Renderer2
} // namespace Luma