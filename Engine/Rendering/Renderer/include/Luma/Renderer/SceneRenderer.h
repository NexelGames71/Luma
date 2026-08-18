#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"

// Scene view and renderer interface. Inspired by UE5's scene view system
// but adapted for Luma's simpler architecture. Provides the bridge between
// the ECS scene system and the rendering pipeline.

namespace Luma {
namespace Renderer2 {

using std::string;
using std::vector;
using std::unique_ptr;
using std::unordered_map;
using namespace Luma::Math;

// Forward declarations
class SceneView;
class PrimitiveSceneInfo;
class StaticMeshBatch;

// ============================================================================
// Scene View
// ============================================================================

// Scene view configuration
struct SceneViewConfig {
    u32 width = 1920;
    u32 height = 1080;
    f32 fov = 60.0f;
    f32 nearPlane = 0.1f;
    f32 farPlane = 10000.0f;
    Mat4 viewMatrix = Mat4::Identity();
    Mat4 projectionMatrix = Mat4::Identity();
    Mat4 viewProjectionMatrix = Mat4::Identity();
    Vec3 cameraPosition = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 cameraDirection = Vec3(0.0f, 0.0f, -1.0f);
    f32 time = 0.0f;
    f32 deltaTime = 0.0f;
};

// Scene view class
class SceneView {
public:
    SceneView();
    ~SceneView();
    
    // Get view configuration
    const SceneViewConfig& GetConfig() const { return m_config; }
    
    // Set view configuration
    void SetConfig(const SceneViewConfig& config) { m_config = config; }
    
    // Update view matrices
    void UpdateMatrices();
    
    // Get view matrix
    const Mat4& GetViewMatrix() const { return m_config.viewMatrix; }
    
    // Get projection matrix
    const Mat4& GetProjectionMatrix() const { return m_config.projectionMatrix; }
    
    // Get view-projection matrix
    const Mat4& GetViewProjectionMatrix() const { return m_config.viewProjectionMatrix; }
    
    // Get camera position
    const Vec3& GetCameraPosition() const { return m_config.cameraPosition; }
    
    // Get camera direction
    const Vec3& GetCameraDirection() const { return m_config.cameraDirection; }
    
    // Get view dimensions
    u32 GetWidth() const { return m_config.width; }
    u32 GetHeight() const { return m_config.height; }
    
    // Set view dimensions
    void SetDimensions(u32 width, u32 height);
    
    // Set camera transform
    void SetCameraTransform(const Vec3& position, const Vec3& direction, const Vec3& up = Vec3(0.0f, 1.0f, 0.0f));
    
    // Set camera perspective
    void SetPerspective(f32 fov, f32 nearPlane, f32 farPlane);
    
    // Update time
    void SetTime(f32 time, f32 deltaTime);
    
private:
    SceneViewConfig m_config;
};

// ============================================================================
// Scene Renderer
// ============================================================================

// Scene renderer configuration
struct SceneRendererConfig {
    bool enableShadows = true;
    bool enablePostProcessing = true;
    bool enableToneMapping = true;
    bool enableBloom = false;
    u32 shadowMapSize = 2048;
    u32 maxLights = 16;
    u32 maxDecals = 32;
};

// Scene renderer interface
class SceneRenderer {
public:
    SceneRenderer();
    virtual ~SceneRenderer();
    
    // Get renderer configuration
    const SceneRendererConfig& GetConfig() const { return m_config; }
    
    // Set renderer configuration
    void SetConfig(const SceneRendererConfig& config) { m_config = config; }
    
    // Add scene view
    void AddSceneView(SceneView* view);
    
    // Remove scene view
    void RemoveSceneView(SceneView* view);
    
    // Get scene view by index
    SceneView* GetSceneView(u32 index) const;
    
    // Get all scene views
    const vector<SceneView*>& GetSceneViews() const { return m_sceneViews; }
    
    // Add primitive to scene
    void AddPrimitive(PrimitiveSceneInfo* primitive);
    
    // Remove primitive from scene
    void RemovePrimitive(PrimitiveSceneInfo* primitive);
    
    // Get all primitives
    const vector<PrimitiveSceneInfo*>& GetPrimitives() const { return m_primitives; }
    
    // Render scene from a specific view
    virtual void RenderScene(SceneView* view) = 0;
    
    // Render all views
    virtual void RenderAllViews();
    
    // Prepare scene for rendering
    virtual void PrepareScene();
    
    // Clear scene
    void ClearScene();
    
    // Get statistics
    u32 GetPrimitiveCount() const { return static_cast<u32>(m_primitives.size()); }
    u32 GetViewCount() const { return static_cast<u32>(m_sceneViews.size()); }
    
protected:
    SceneRendererConfig m_config;
    vector<SceneView*> m_sceneViews;
    vector<PrimitiveSceneInfo*> m_primitives;
    
    // Prepare view for rendering
    virtual void PrepareView(SceneView* view);
    
    // Cull primitives for view
    virtual void CullPrimitives(SceneView* view);
};

// ============================================================================
// Scene Renderer Factory
// ============================================================================

// Factory for creating scene renderers
class SceneRendererFactory {
public:
    static SceneRenderer* CreateRenderer(const string& rendererType);
    
    // Register custom renderer type
    static void RegisterRenderer(const string& type, SceneRenderer* (*creator)());
    
    // Get available renderer types
    static vector<string> GetAvailableRendererTypes();
    
private:
    static unordered_map<string, SceneRenderer* (*)()> s_rendererCreators;
};

} // namespace Renderer2
} // namespace Luma