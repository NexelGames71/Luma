#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Luma/Editor/Panels/Console.h"
#include "Luma/Editor/Panels/Inspector.h"
#include "Luma/Editor/Panels/Viewport.h"
#include "Luma/Editor/Panels/WorldOutliner.h"
#include "PanelContext.h"
#include "Luma/Gizmo/TranslateGizmo.h"
#include "Luma/Math/Math.h"
#include "Luma/Project/Project.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/Scene/Scene.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/DockSpace.h"

namespace Luma {

// The editor shell: menu bar, toolbar, and a dockable layout (World Outliner,
// Viewport, Inspector, Console). Drives an ECS Scene (EnTT) and orchestrates
// the per-panel modules under Editor/panels/. The panels themselves are
// self-contained; this class only wires them together + owns the shared
// state (scene, camera, gizmo, selection).
class EditorScreen {
public:
    explicit EditorScreen(const std::filesystem::path& projectFile);

    bool HasProject() const { return m_project.has_value(); }

    // Builds the scene view (camera + grid + entity instances + gizmo overlay).
    SceneView BuildSceneView();

    void Draw(Slate::Context& ui, f32 width, f32 height);

    Slate::Rect ViewportRect() const { return m_viewportPanel.Rect(); }
    void SetViewportTexture(TextureHandle texture) {
        m_viewportPanel.SetTexture(texture);
    }

    void SetToolbarIcons(TextureHandle play, TextureHandle pause,
                         TextureHandle stop) {
        m_iconPlay = play;
        m_iconPause = pause;
        m_iconStop = stop;
    }
    void SetLogoIcon(TextureHandle logo) { m_iconLogo = logo; }

private:
    void AddEntity();
    void CreateEnvironment();
    bool LoadScene();
    void SaveScene();
    void BuildDock();

    // ---- Shared state the panels read/write via PanelContext ----------------
    std::optional<Project> m_project;
    std::string m_title;

    Scene m_scene;
    Entity m_selected = kNullEntity;
    Entity m_environment = kNullEntity;
    int m_nextNumber = 1;

    TranslateGizmo m_gizmo;
    std::vector<SceneInstance> m_instances;
    std::vector<SceneLight> m_lights;

    f32 m_camYaw = 0.9f;
    f32 m_camPitch = 0.5f;
    f32 m_camDistance = 12.0f;
    Math::Vec3 m_camTarget{0.0f, 0.0f, 0.0f};

    Math::Mat4 m_view = Math::Mat4::Identity();
    f32 m_fovY = 0.9f;
    f32 m_nearZ = 0.1f;
    f32 m_farZ = 500.0f;
    f32 m_gizmoScale = 1.0f;

    Slate::DockSpace m_dock;
    bool m_dockBuilt = false;

    bool m_showFileMenu = false;
    f32 m_fileMenuX = 42.0f;

    TextureHandle m_iconPlay = 0;
    TextureHandle m_iconPause = 0;
    TextureHandle m_iconStop = 0;
    TextureHandle m_iconLogo = 0;

    // ---- Panels (one per docked pane) ---------------------------------------
    Editor::Panels::WorldOutlinerPanel m_outlinerPanel;
    Editor::Panels::ViewportPanel m_viewportPanel;
    Editor::Panels::InspectorPanel m_inspectorPanel;
    Editor::Panels::ConsolePanel m_consolePanel;

    Editor::Panels::PanelContext BuildPanelContext();
};

}  // namespace Luma
