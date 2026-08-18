#pragma once

#include "Luma/Renderer/SceneRenderer.h"
#include "Luma/RHI/RHIContext.h"
#include "Luma/RHI/RHICommandList.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/Core/Types.h"

// Deferred shading renderer. Combines GBuffer, lighting, and shadow systems
// to implement deferred shading rendering pipeline.

namespace Luma {
namespace Renderer2 {

// Editor rendering modes
enum class EditorRenderMode : u8 {
    Deferred,      // Full deferred shading
    Unlit,         // No lighting, just albedo
    Wireframe,     // Wireframe rendering
    Depth,         // Depth buffer visualization
    Normal,        // Normal buffer visualization
    GBufferAlbedo, // GBuffer albedo visualization
    GBufferNormal, // GBuffer normal visualization
    GBufferMaterial, // GBuffer material visualization
    LightingOnly   // Lighting pass only
};

// Forward declarations
class GBuffer;
class GBufferRenderer;
class LightingRenderer;
class ShadowRenderer;
class LightAccumulationBuffer;

// ============================================================================
// Deferred Scene View
// ============================================================================

// Scene view specifically for deferred rendering
struct DeferredSceneView {
    Math::Vec3 cameraPosition{0.0f, 0.0f, 0.0f};
    Math::Vec3 cameraDirection{0.0f, 0.0f, -1.0f};
    Math::Mat4 viewMatrix = Math::Mat4::Identity();
    Math::Mat4 projectionMatrix = Math::Mat4::Identity();
    Math::Mat4 viewProjectionMatrix = Math::Mat4::Identity();
    f32 fov = 0.9f;
    f32 nearPlane = 0.1f;
    f32 farPlane = 500.0f;
    u32 width = 1920;
    u32 height = 1080;
    f32 time = 0.0f;
    f32 deltaTime = 0.0f;

    // Pointer to the RHI scene data (instances, lights, etc.)
    // Filled by the editor before calling RenderScene.
    const Luma::SceneView* sceneData = nullptr;
    const Luma::LightingParams* lightingParams = nullptr;
};

// ============================================================================
// Performance Monitoring
// ============================================================================

// Performance metrics for deferred renderer
struct DeferredRendererStats {
    f32 gBufferTime = 0.0f;
    f32 shadowTime = 0.0f;
    f32 lightingTime = 0.0f;
    f32 compositionTime = 0.0f;
    f32 totalTime = 0.0f;
    u32 drawCalls = 0;
    u32 triangles = 0;
};

// ============================================================================
// Deferred Shading Renderer
// ============================================================================

// Deferred shading renderer implementation
class DeferredShadingRenderer {
public:
    DeferredShadingRenderer();
    ~DeferredShadingRenderer();
    
    // Initialize with RHI device
    bool Initialize(RHI::RHIDevice* device);
    
    // Render scene using deferred shading
    void RenderScene(const DeferredSceneView& view);
    
    // Prepare scene for deferred rendering
    void PrepareScene();
    
    // Get GBuffer
    GBuffer* GetGBuffer() const { return m_gBuffer; }
    
    // Get lighting renderer
    LightingRenderer* GetLightingRenderer() const { return m_lightingRenderer; }
    
    // Get shadow renderer
    ShadowRenderer* GetShadowRenderer() const { return m_shadowRenderer; }
    
    // Get light accumulation buffer
    LightAccumulationBuffer* GetLightAccumulationBuffer() const { return m_lightAccumulation; }
    
    // Set viewport dimensions
    void SetViewportDimensions(u32 width, u32 height);
    
    // Set editor render mode
    void SetEditorRenderMode(EditorRenderMode mode) { m_editorRenderMode = mode; }
    EditorRenderMode GetEditorRenderMode() const { return m_editorRenderMode; }
    
    // Toggle debug visualization
    void SetDebugVisualization(bool enabled) { m_debugVisualization = enabled; }
    bool GetDebugVisualization() const { return m_debugVisualization; }
    
    // Performance monitoring
    const DeferredRendererStats& GetStats() const { return m_stats; }
    void ResetStats() { m_stats = {}; }
    void UpdateStats();
    
private:
    // Cleanup helper for initialization rollback
    void Cleanup();
    
private:
    GBuffer* m_gBuffer = nullptr;
    GBufferRenderer* m_gbufferRenderer = nullptr;
    LightingRenderer* m_lightingRenderer = nullptr;
    ShadowRenderer* m_shadowRenderer = nullptr;
    LightAccumulationBuffer* m_lightAccumulation = nullptr;
    RHI::RHIDevice* m_device = nullptr;
    RHI::RHICommandList* m_commandList = nullptr;
    u32 m_viewportWidth = 1920;
    u32 m_viewportHeight = 1080;
    EditorRenderMode m_editorRenderMode = EditorRenderMode::Deferred;
    bool m_debugVisualization = false;
    bool m_initialized = false;
    DeferredRendererStats m_stats;
    
    // GBuffer pass
    void RenderGBufferPass(const DeferredSceneView& view);
    
    // Lighting pass
    void RenderLightingPass(const DeferredSceneView& view);
    
    // Shadow pass
    void RenderShadowPass(const DeferredSceneView& view);
    
    // Final composition pass
    void RenderCompositionPass(const DeferredSceneView& view);
    
    // Editor-specific visualization modes
    void RenderEditorVisualization(const DeferredSceneView& view);
    void RenderWireframe(const DeferredSceneView& view);
    void RenderUnlit(const DeferredSceneView& view);
    void RenderDebugGBuffer(const DeferredSceneView& view);
};

// ============================================================================
// Deferred Renderer Factory
// ============================================================================

// Factory function to create deferred shading renderer
DeferredShadingRenderer* CreateDeferredRenderer();

} // namespace Renderer2
} // namespace Luma