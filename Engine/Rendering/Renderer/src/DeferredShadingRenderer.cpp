#include "Luma/Renderer/DeferredShadingRenderer.h"
#include "Luma/Renderer/GBufferInfo.h"
#include "Luma/Renderer/Lighting.h"
#include "Luma/Renderer/Shadows.h"

#include <string>
#include <exception>

#include "Luma/Core/Log.h"
#include "Luma/RHI/RHIContext.h"

namespace Luma {
namespace Renderer2 {

using std::string;

// ============================================================================
// Deferred Shading Renderer
// ============================================================================

// Deferred shading renderer implementation
DeferredShadingRenderer::DeferredShadingRenderer()
    : m_device(nullptr) {
}

bool DeferredShadingRenderer::Initialize(RHI::RHIDevice* device) {
    if (!device) {
        LUMA_LOG_ERROR("Deferred", "Initialize called with a null RHIDevice; "
                                     "wire a VulkanRHIDevice adapter first");
        return false;
    }
    
    // Validate context
    auto* context = device->GetContext();
    if (!context) {
        LUMA_LOG_ERROR("Deferred", "Initialize: device has no RHI context");
        return false;
    }
    
    // Validate graphics queue
    auto* graphicsQueue = context->GetGraphicsQueue();
    if (!graphicsQueue) {
        LUMA_LOG_ERROR("Deferred", "Initialize: graphics queue is null");
        return false;
    }
    
    // Validate viewport dimensions
    if (m_viewportWidth == 0 || m_viewportHeight == 0) {
        LUMA_LOG_WARN("Deferred", "Initialize: viewport dimensions are zero, using defaults");
        m_viewportWidth = 1920;
        m_viewportHeight = 1080;
    }
    
    m_device = device;

    // Create a command list for the deferred passes
    m_commandList = context->CreateCommandList();
    if (!m_commandList) {
        LUMA_LOG_ERROR("Deferred", "Failed to create RHI command list");
        return false;
    }

    m_gBuffer = new GBuffer();
    GBufferDesc gbufferDesc;
    gbufferDesc.width  = m_viewportWidth;
    gbufferDesc.height = m_viewportHeight;
    if (!m_gBuffer->Create(gbufferDesc, device)) {
        delete m_gBuffer;
        m_gBuffer = nullptr;
        LUMA_LOG_ERROR("Deferred", "Failed to create GBuffer");
        return false;
    }

    // GBuffer renderer - make this fatal
    m_gbufferRenderer = new GBufferRenderer();
    m_gbufferRenderer->SetGBuffer(m_gBuffer);
    if (!m_gbufferRenderer->Initialize(device)) {
        delete m_gbufferRenderer;
        m_gbufferRenderer = nullptr;
        delete m_gBuffer;
        m_gBuffer = nullptr;
        LUMA_LOG_ERROR("Deferred", "GBufferRenderer failed to init (shaders missing?)");
        return false;
    }

    // Create light accumulation buffer - make this fatal
    m_lightAccumulation = new LightAccumulationBuffer();
    if (!m_lightAccumulation->Create(m_viewportWidth, m_viewportHeight, RHI::ETextureFormat::R8G8B8A8_UNORM, device)) {
        delete m_lightAccumulation;
        m_lightAccumulation = nullptr;
        delete m_gbufferRenderer;
        m_gbufferRenderer = nullptr;
        delete m_gBuffer;
        m_gBuffer = nullptr;
        LUMA_LOG_ERROR("Deferred", "Failed to create light accumulation buffer");
        return false;
    }

    // Lighting renderer - make this fatal
    m_lightingRenderer = new LightingRenderer();
    m_lightingRenderer->SetGBuffer(m_gBuffer);
    if (!m_lightingRenderer->Initialize(device, m_lightAccumulation)) {
        delete m_lightingRenderer;
        m_lightingRenderer = nullptr;
        delete m_lightAccumulation;
        m_lightAccumulation = nullptr;
        delete m_gbufferRenderer;
        m_gbufferRenderer = nullptr;
        delete m_gBuffer;
        m_gBuffer = nullptr;
        LUMA_LOG_ERROR("Deferred", "LightingRenderer failed to init (shaders missing?)");
        return false;
    }

    // Shadow renderer (not initialized yet - stub)
    m_shadowRenderer = new ShadowRenderer();

    LUMA_LOG_INFO("Deferred",
                   "Initialized with RHI device ({}) — GBuffer is {}x{}",
                   device->GetDeviceName(), m_viewportWidth, m_viewportHeight);
    m_initialized = true;
    return true;
}

DeferredShadingRenderer::~DeferredShadingRenderer() {
    Cleanup();
}

void DeferredShadingRenderer::Cleanup() {
    if (m_gbufferRenderer) {
        delete m_gbufferRenderer;
        m_gbufferRenderer = nullptr;
    }
    if (m_lightingRenderer) {
        delete m_lightingRenderer;
        m_lightingRenderer = nullptr;
    }
    if (m_shadowRenderer) {
        delete m_shadowRenderer;
        m_shadowRenderer = nullptr;
    }
    if (m_lightAccumulation) {
        m_lightAccumulation->Destroy();
        delete m_lightAccumulation;
        m_lightAccumulation = nullptr;
    }
    if (m_gBuffer) {
        m_gBuffer->Destroy(m_device);
        delete m_gBuffer;
        m_gBuffer = nullptr;
    }
    // Note: m_commandList is owned by context, don't delete here
    m_commandList = nullptr;
    
    m_initialized = false;
}

void DeferredShadingRenderer::RenderScene(const DeferredSceneView& view) {
    if (!m_gBuffer || !m_gBuffer->IsValid()) {
        LUMA_LOG_ERROR("Deferred", "RenderScene: GBuffer is invalid");
        return;
    }
    
    LUMA_LOG_INFO("Deferred", "RenderScene: Starting render pipeline");
    
    // Render based on editor mode
    if (m_debugVisualization) {
        LUMA_LOG_INFO("Deferred", "RenderScene: Rendering editor visualization");
        RenderEditorVisualization(view);
    } else {
        // Standard deferred rendering pipeline
        LUMA_LOG_INFO("Deferred", "RenderScene: Rendering GBuffer pass");
        RenderGBufferPass(view);
        LUMA_LOG_INFO("Deferred", "RenderScene: Rendering Shadow pass");
        RenderShadowPass(view);
        LUMA_LOG_INFO("Deferred", "RenderScene: Rendering Lighting pass");
        RenderLightingPass(view);
        LUMA_LOG_INFO("Deferred", "RenderScene: Rendering Composition pass");
        RenderCompositionPass(view);
    }
    
    LUMA_LOG_INFO("Deferred", "RenderScene: Completed render pipeline");
}

void DeferredShadingRenderer::PrepareScene() {
    if (!m_device) {
        LUMA_LOG_ERROR("Deferred", "PrepareScene: device is null");
        return;
    }
    
    // Prepare scene for deferred rendering
    // Update GBuffer dimensions if needed
    if (m_gBuffer && (m_gBuffer->GetWidth() != m_viewportWidth || m_gBuffer->GetHeight() != m_viewportHeight)) {
        LUMA_LOG_INFO("Deferred", "PrepareScene: Resizing GBuffer from {}x{} to {}x{}",
                     m_gBuffer->GetWidth(), m_gBuffer->GetHeight(), m_viewportWidth, m_viewportHeight);
        // Wait for GPU to finish using old resources before resizing
        m_device->WaitIdle();
        m_gBuffer->Resize(m_viewportWidth, m_viewportHeight, m_device);
    }
    
    // Update light accumulation buffer if needed
    if (m_lightAccumulation) {
        // Assuming LightAccumulationBuffer has GetWidth/GetHeight methods
        // If not, we'll need to track dimensions separately
        m_device->WaitIdle();
        m_lightAccumulation->Resize(m_viewportWidth, m_viewportHeight, m_device);
    }
}

void DeferredShadingRenderer::SetViewportDimensions(u32 width, u32 height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
}

void DeferredShadingRenderer::RenderGBufferPass(const DeferredSceneView& view) {
    LUMA_LOG_INFO("Deferred", "RenderGBufferPass: Checking prerequisites");
    
    if (!m_gbufferRenderer) {
        LUMA_LOG_ERROR("Deferred", "RenderGBufferPass: GBuffer renderer is null");
        return;
    }
    
    if (!m_commandList) {
        LUMA_LOG_ERROR("Deferred", "RenderGBufferPass: Command list is null");
        return;
    }
    
    if (!view.sceneData) {
        LUMA_LOG_ERROR("Deferred", "RenderGBufferPass: Scene data is null");
        return;
    }
    
    if (!m_gBuffer || !m_gBuffer->IsValid()) {
        LUMA_LOG_ERROR("Deferred", "RenderGBufferPass: GBuffer is invalid");
        return;
    }

    LUMA_LOG_INFO("Deferred", "RenderGBufferPass: Starting");
    
    try {
        m_commandList->Begin();
        LUMA_LOG_INFO("Deferred", "RenderGBufferPass: Command list begun");
        
        m_gbufferRenderer->Prepare(m_commandList);
        LUMA_LOG_INFO("Deferred", "RenderGBufferPass: Prepare completed");
        
        m_gbufferRenderer->SetRenderTargets(m_commandList);
        LUMA_LOG_INFO("Deferred", "RenderGBufferPass: Render targets set");
        
        m_gbufferRenderer->RenderGeometry(m_commandList, *view.sceneData);
        LUMA_LOG_INFO("Deferred", "RenderGBufferPass: Geometry rendered");
        
        // End the dynamic rendering pass before transitioning
        // (VulkanRHICommandList::End closes any open render pass)
        m_commandList->End();
        LUMA_LOG_INFO("Deferred", "RenderGBufferPass: Command list ended");
        
        LUMA_LOG_INFO("Deferred", "RenderGBufferPass: Submitting command list");
        {
            RHI::RHICommandList* lists[] = {m_commandList};
            m_device->GetContext()->GetGraphicsQueue()->Submit(1, lists);
        }
        
        LUMA_LOG_INFO("Deferred", "RenderGBufferPass: Waiting for GPU");
        m_device->WaitIdle();
        
        LUMA_LOG_INFO("Deferred", "RenderGBufferPass: Completed");
    } catch (const std::exception& e) {
        LUMA_LOG_ERROR("Deferred", "RenderGBufferPass: Exception: {}", e.what());
    } catch (...) {
        LUMA_LOG_ERROR("Deferred", "RenderGBufferPass: Unknown exception");
    }
}

void DeferredShadingRenderer::RenderLightingPass(const DeferredSceneView& view) {
    if (!m_lightingRenderer || !m_commandList) return;

    LUMA_LOG_INFO("Deferred", "RenderLightingPass: Starting");
    
    try {
        // Validate lights array
        if (view.sceneData && view.sceneData->lightCount > 0 && !view.sceneData->lights) {
            LUMA_LOG_ERROR("Deferred", "RenderLightingPass: lightCount > 0 but lights array is null");
            return;
        }

        // Populate lights from the scene's LightingParams / SceneLights
        if (view.lightingParams) {
            m_lightingRenderer->SetLightingParameters(
                LightingParameters::FromLightingParams(*view.lightingParams));
        }
        if (view.sceneData) {
            m_lightingRenderer->ClearLights();
            for (u32 i = 0; i < view.sceneData->lightCount; ++i) {
                m_lightingRenderer->AddLight(LightData::FromSceneLight(view.sceneData->lights[i]));
            }
        }

        m_commandList->Begin();
        LUMA_LOG_INFO("Deferred", "RenderLightingPass: Command list begun");
        
        m_lightingRenderer->Prepare(m_commandList);   // GBuffer → ShaderResource
        LUMA_LOG_INFO("Deferred", "RenderLightingPass: Prepare completed");
        
        m_lightingRenderer->RenderLighting(m_commandList);
        LUMA_LOG_INFO("Deferred", "RenderLightingPass: Lighting rendered");
        
        m_commandList->End();
        LUMA_LOG_INFO("Deferred", "RenderLightingPass: Command list ended");
        
        LUMA_LOG_INFO("Deferred", "RenderLightingPass: Submitting command list");
        {
            RHI::RHICommandList* lists[] = {m_commandList};
            m_device->GetContext()->GetGraphicsQueue()->Submit(1, lists);
        }
        
        LUMA_LOG_INFO("Deferred", "RenderLightingPass: Waiting for GPU");
        m_device->WaitIdle();
        
        LUMA_LOG_INFO("Deferred", "RenderLightingPass: Completed");
    } catch (const std::exception& e) {
        LUMA_LOG_ERROR("Deferred", "RenderLightingPass: Exception: {}", e.what());
    } catch (...) {
        LUMA_LOG_ERROR("Deferred", "RenderLightingPass: Unknown exception");
    }
}

void DeferredShadingRenderer::RenderShadowPass(const DeferredSceneView& view) {
    // TODO: Render shadow pass
    // This would use the ShadowRenderer to render shadow maps
    (void)view;
}

void DeferredShadingRenderer::RenderCompositionPass(const DeferredSceneView& view) {
    // The lighting shader already handles composition (tone mapping, gamma correction)
    // The light accumulation buffer already contains the final composed result
    // We just need to ensure it's in the right state for consumption
    (void)view; // Unused parameter - composition is handled by lighting shader
    
    LUMA_LOG_INFO("Deferred", "RenderCompositionPass: Starting");
    
    if (!m_lightAccumulation || !m_commandList) {
        LUMA_LOG_ERROR("Deferred", "RenderCompositionPass: Light accumulation buffer or command list is null");
        return;
    }
    
    try {
        m_commandList->Begin();
        LUMA_LOG_INFO("Deferred", "RenderCompositionPass: Command list begun");
        
        // Ensure the light accumulation buffer is in shader-read state for external consumption
        m_commandList->ResourceBarrier(m_lightAccumulation->GetAccumulationTexture(), RHI::EResourceState::ShaderResource);
        LUMA_LOG_INFO("Deferred", "RenderCompositionPass: Resource barrier completed");
        
        m_commandList->End();
        LUMA_LOG_INFO("Deferred", "RenderCompositionPass: Command list ended");
        
        LUMA_LOG_INFO("Deferred", "RenderCompositionPass: Submitting command list");
        {
            RHI::RHICommandList* lists[] = {m_commandList};
            m_device->GetContext()->GetGraphicsQueue()->Submit(1, lists);
        }
        
        LUMA_LOG_INFO("Deferred", "RenderCompositionPass: Waiting for GPU");
        m_device->WaitIdle();
        
        LUMA_LOG_INFO("Deferred", "RenderCompositionPass: Completed");
    } catch (const std::exception& e) {
        LUMA_LOG_ERROR("Deferred", "RenderCompositionPass: Exception: {}", e.what());
    } catch (...) {
        LUMA_LOG_ERROR("Deferred", "RenderCompositionPass: Unknown exception");
    }
}

void DeferredShadingRenderer::RenderEditorVisualization(const DeferredSceneView& view) {
    switch (m_editorRenderMode) {
        case EditorRenderMode::Deferred:
            // Normal deferred rendering pipeline
            RenderGBufferPass(view);
            RenderShadowPass(view);
            RenderLightingPass(view);
            RenderCompositionPass(view);
            break;
            
        case EditorRenderMode::Unlit:
            RenderUnlit(view);
            break;
            
        case EditorRenderMode::Wireframe:
            RenderWireframe(view);
            break;
            
        case EditorRenderMode::Depth:
        case EditorRenderMode::Normal:
        case EditorRenderMode::GBufferAlbedo:
        case EditorRenderMode::GBufferNormal:
        case EditorRenderMode::GBufferMaterial:
            RenderDebugGBuffer(view);
            break;
            
        case EditorRenderMode::LightingOnly:
            RenderGBufferPass(view);
            RenderShadowPass(view);
            RenderLightingPass(view);
            // Output lighting pass directly without composition
            break;
            
        default:
            // Default to deferred
            RenderGBufferPass(view);
            RenderShadowPass(view);
            RenderLightingPass(view);
            RenderCompositionPass(view);
            break;
    }
}

void DeferredShadingRenderer::RenderWireframe(const DeferredSceneView& view) {
    // TODO: Render scene in wireframe mode
    // This would:
    // 1. Set rasterizer state to wireframe
    // 2. Render geometry with basic color
    // 3. No lighting or materials
    (void)view;
}

void DeferredShadingRenderer::RenderUnlit(const DeferredSceneView& view) {
    // TODO: Render scene without lighting
    // This would:
    // 1. Render geometry with albedo only
    // 2. No lighting calculations
    // 3. Useful for checking texture quality
    (void)view;
}

void DeferredShadingRenderer::RenderDebugGBuffer(const DeferredSceneView& view) {
    // TODO: Render GBuffer visualization based on mode
    // This would:
    // 1. For Depth: render depth buffer as grayscale
    // 2. For Normal: render normals as colors
    // 3. For GBufferAlbedo: render albedo target
    // 4. For GBufferNormal: render normal target
    // 5. For GBufferMaterial: render material properties
    (void)view;
}

void DeferredShadingRenderer::UpdateStats() {
    // TODO: Update performance statistics
    // This would:
    // 1. Measure GPU timing for each pass
    // 2. Count draw calls and triangles
    // 3. Calculate total frame time
    // For now, this is a stub implementation
}

// ============================================================================
// Deferred Renderer Factory
// ============================================================================

// Factory function to create deferred shading renderer
DeferredShadingRenderer* CreateDeferredRenderer() {
    return new DeferredShadingRenderer();
}

} // namespace Renderer2
} // namespace Luma