#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Luma/Gizmo/TranslateGizmo.h"
#include "Luma/Grid/Grid.h"
#include "Luma/Math/Math.h"
#include "Luma/Project/Project.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/Scene/Scene.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/DockSpace.h"

namespace Luma {

// The editor shell: menu bar, toolbar, and a dockable layout (World Outliner,
// Viewport, Inspector, Console). Drives an ECS Scene (EnTT): the outliner lists
// entities, the viewport renders them and hosts a translate gizmo, the inspector
// shows the selection.
class EditorScreen {
public:
    explicit EditorScreen(const std::filesystem::path& projectFile);

    bool HasProject() const { return m_project.has_value(); }

    // Builds the scene view (camera + grid + entity instances + gizmo overlay).
    SceneView BuildSceneView();

    void Draw(Slate::Context& ui, f32 width, f32 height);

    Slate::Rect ViewportRect() const { return m_viewportRect; }
    void SetViewportTexture(TextureHandle texture) { m_viewport = texture; }

    void SetToolbarIcons(TextureHandle play, TextureHandle pause,
                         TextureHandle stop) {
        m_iconPlay = play;
        m_iconPause = pause;
        m_iconStop = stop;
    }

private:
    void AddEntity();
    void BuildDock();
    void UpdateCameraAndGizmo(Slate::Context& ui, const Slate::Rect& viewport);
    void DrawOutlinerContent(Slate::Context& ui, const Slate::Rect& rect);
    void DrawViewportContent(Slate::Context& ui, const Slate::Rect& rect);
    void DrawInspectorContent(Slate::Context& ui, const Slate::Rect& rect);
    void DrawConsoleContent(Slate::Context& ui, const Slate::Rect& rect);

    std::optional<Project> m_project;
    std::string m_title;

    Scene m_scene;
    Entity m_selected = kNullEntity;
    int m_nextNumber = 1;

    Grid m_grid;
    TranslateGizmo m_gizmo;
    std::vector<SceneInstance> m_instances;  // rebuilt each frame

    // Orbit camera.
    f32 m_camYaw = 0.9f;
    f32 m_camPitch = 0.5f;
    f32 m_camDistance = 12.0f;
    Math::Vec3 m_camTarget{0.0f, 0.0f, 0.0f};

    // Cached each BuildSceneView for the gizmo's screen projection in Draw.
    Math::Mat4 m_view = Math::Mat4::Identity();
    f32 m_fovY = 0.9f;
    f32 m_nearZ = 0.1f;
    f32 m_farZ = 500.0f;
    f32 m_gizmoScale = 1.0f;

    Slate::DockSpace m_dock;
    bool m_dockBuilt = false;

    Slate::Rect m_viewportRect{};
    TextureHandle m_viewport = 0;
    TextureHandle m_iconPlay = 0;
    TextureHandle m_iconPause = 0;
    TextureHandle m_iconStop = 0;
};

}  // namespace Luma
