#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Luma/Math/Math.h"
#include "Luma/Project/Project.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/Slate/Context.h"

namespace Luma {

// A scene entity: a named transform with a display color. Created in-editor via
// the World Outliner (the scene starts empty).
struct Entity {
    std::string name;
    Math::Vec3 position{0.0f, 1.0f, 0.0f};
    Math::Vec3 rotationDeg{0.0f, 0.0f, 0.0f};
    Math::Vec3 scale{1.0f, 1.0f, 1.0f};
    Math::Vec3 color{0.80f, 0.80f, 0.85f};
};

// The editor shell: menu bar, toolbar, and a dockable layout (World Outliner,
// Viewport, Inspector, Console) with draggable splitters. Owns the scene and an
// orbit camera; the viewport shows the scene rendered by the RHI.
class EditorScreen {
public:
    explicit EditorScreen(const std::filesystem::path& projectFile);

    bool HasProject() const { return m_project.has_value(); }

    // Builds the scene (camera + entity instances) for the renderer.
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
    void DrawOutliner(Slate::Context& ui, const Slate::Rect& rect);
    void DrawInspector(Slate::Context& ui, const Slate::Rect& rect);
    void UpdateCamera(Slate::Context& ui, const Slate::Rect& viewport);
    void AddEntity();
    Math::Mat4 EntityMatrix(const Entity& e) const;

    std::optional<Project> m_project;
    std::string m_title;

    // Scene (starts empty).
    std::vector<Entity> m_entities;
    int m_selected = -1;
    int m_nextEntityNumber = 1;
    std::vector<SceneInstance> m_instances;  // rebuilt each BuildSceneView

    // Orbit camera.
    f32 m_camYaw = 0.9f;
    f32 m_camPitch = 0.5f;
    f32 m_camDistance = 10.0f;
    Math::Vec3 m_camTarget{0.0f, 1.0f, 0.0f};

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
