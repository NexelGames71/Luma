#include "Luma/Renderer/SceneRenderer.h"

#include <algorithm>
#include <string>

namespace Luma {
namespace Renderer2 {

using std::string;
using namespace Luma::Math;

// ============================================================================
// Scene View
// ============================================================================

SceneView::SceneView() {
    m_config.viewMatrix = Mat4::Identity();
    m_config.projectionMatrix = Mat4::Identity();
    m_config.viewProjectionMatrix = Mat4::Identity();
}

SceneView::~SceneView() {
}

void SceneView::UpdateMatrices() {
    m_config.viewProjectionMatrix = m_config.projectionMatrix * m_config.viewMatrix;
}

void SceneView::SetDimensions(u32 width, u32 height) {
    m_config.width = width;
    m_config.height = height;
    // Recalculate projection matrix if needed
    UpdateMatrices();
}

void SceneView::SetCameraTransform(const Vec3& position, const Vec3& direction, const Vec3& up) {
    m_config.cameraPosition = position;
    m_config.cameraDirection = direction;
    
    // Build view matrix
    Vec3 target = position + direction;
    m_config.viewMatrix = LookAt(position, target, up);
    
    UpdateMatrices();
}

void SceneView::SetPerspective(f32 fov, f32 nearPlane, f32 farPlane) {
    m_config.fov = fov;
    m_config.nearPlane = nearPlane;
    m_config.farPlane = farPlane;
    
    f32 aspect = static_cast<f32>(m_config.width) / static_cast<f32>(m_config.height);
    m_config.projectionMatrix = Perspective(Radians(fov), aspect, nearPlane, farPlane);
    
    UpdateMatrices();
}

void SceneView::SetTime(f32 time, f32 deltaTime) {
    m_config.time = time;
    m_config.deltaTime = deltaTime;
}

// ============================================================================
// Scene Renderer
// ============================================================================

SceneRenderer::SceneRenderer() {
}

SceneRenderer::~SceneRenderer() {
    ClearScene();
}

void SceneRenderer::AddSceneView(SceneView* view) {
    if (view) {
        m_sceneViews.push_back(view);
    }
}

void SceneRenderer::RemoveSceneView(SceneView* view) {
    auto it = std::find(m_sceneViews.begin(), m_sceneViews.end(), view);
    if (it != m_sceneViews.end()) {
        m_sceneViews.erase(it);
    }
}

SceneView* SceneRenderer::GetSceneView(u32 index) const {
    if (index < m_sceneViews.size()) {
        return m_sceneViews[index];
    }
    return nullptr;
}

void SceneRenderer::AddPrimitive(PrimitiveSceneInfo* primitive) {
    if (primitive) {
        m_primitives.push_back(primitive);
    }
}

void SceneRenderer::RemovePrimitive(PrimitiveSceneInfo* primitive) {
    auto it = std::find(m_primitives.begin(), m_primitives.end(), primitive);
    if (it != m_primitives.end()) {
        m_primitives.erase(it);
    }
}

void SceneRenderer::RenderAllViews() {
    for (auto* view : m_sceneViews) {
        RenderScene(view);
    }
}

void SceneRenderer::PrepareScene() {
    // Prepare scene for rendering
    // TODO: Implement scene preparation
}

void SceneRenderer::ClearScene() {
    m_sceneViews.clear();
    m_primitives.clear();
}

void SceneRenderer::PrepareView(SceneView* view) {
    if (!view) {
        return;
    }
    
    // Prepare view for rendering
    view->UpdateMatrices();
}

void SceneRenderer::CullPrimitives(SceneView* view) {
    if (!view) {
        return;
    }
    
    // Cull primitives for the view
    // TODO: Implement culling
    (void)view;
}

// ============================================================================
// Scene Renderer Factory
// ============================================================================

unordered_map<string, SceneRenderer* (*)()> SceneRendererFactory::s_rendererCreators;

SceneRenderer* SceneRendererFactory::CreateRenderer(const string& rendererType) {
    auto it = s_rendererCreators.find(rendererType);
    if (it != s_rendererCreators.end()) {
        return it->second();
    }
    return nullptr;
}

void SceneRendererFactory::RegisterRenderer(const string& type, SceneRenderer* (*creator)()) {
    s_rendererCreators[type] = creator;
}

vector<string> SceneRendererFactory::GetAvailableRendererTypes() {
    vector<string> types;
    for (const auto& [type, creator] : s_rendererCreators) {
        types.push_back(type);
    }
    return types;
}

} // namespace Renderer2
} // namespace Luma