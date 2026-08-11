#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "Luma/Grid/Grid.h"
#include "Luma/Math/Math.h"
#include "Luma/Project/Project.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/Slate/Context.h"

namespace Luma {

// The editor shell: menu bar, toolbar, and a dockable layout (World Outliner,
// Viewport, Inspector, Console). Owns the viewport camera and the ground grid.
//
// The scene/entity model is intentionally NOT built here - it will be provided
// by the ECS (EnTT) when that lands; the outliner/inspector are wired to real
// entities then. For now the viewport shows the grid with orbit navigation.
class EditorScreen {
public:
    explicit EditorScreen(const std::filesystem::path& projectFile);

    bool HasProject() const { return m_project.has_value(); }

    // Builds the scene view (camera + ground grid) for the renderer.
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
    void UpdateCamera(Slate::Context& ui, const Slate::Rect& viewport);

    std::optional<Project> m_project;
    std::string m_title;

    Grid m_grid;  // ground grid line geometry (Luma::Grid module)

    // Orbit camera.
    f32 m_camYaw = 0.9f;
    f32 m_camPitch = 0.5f;
    f32 m_camDistance = 12.0f;
    Math::Vec3 m_camTarget{0.0f, 0.0f, 0.0f};

    // Dock split ratios (draggable; persist).
    f32 m_consoleSplit = 0.74f;
    f32 m_leftSplit = 0.2f;
    f32 m_rightSplit = 0.78f;

    Slate::Rect m_viewportRect{};
    TextureHandle m_viewport = 0;
    TextureHandle m_iconPlay = 0;
    TextureHandle m_iconPause = 0;
    TextureHandle m_iconStop = 0;
};

}  // namespace Luma
