#pragma once

#include <filesystem>
#include <optional>

#include "Luma/Project/Project.h"
#include "Luma/Slate/Context.h"

namespace Luma {

// The editor shell: menu bar, toolbar, and a dockable layout (World Outliner,
// Viewport, Inspector, Console) with draggable splitters. The center viewport
// can display a rendered scene texture set via SetViewportTexture.
class EditorScreen {
public:
    explicit EditorScreen(const std::filesystem::path& projectFile);

    bool HasProject() const { return m_project.has_value(); }
    void Draw(Slate::Context& ui, f32 width, f32 height);

    // The center viewport's content rect from the last Draw (pixels).
    Slate::Rect ViewportRect() const { return m_viewportRect; }
    void SetViewportTexture(TextureHandle texture) { m_viewport = texture; }

    void SetToolbarIcons(TextureHandle play, TextureHandle pause,
                         TextureHandle stop) {
        m_iconPlay = play;
        m_iconPause = pause;
        m_iconStop = stop;
    }

private:
    std::optional<Project> m_project;
    std::string m_title;

    // Dock split ratios (persist across frames; draggable).
    f32 m_consoleSplit = 0.74f;  // main area vs. bottom console
    f32 m_leftSplit = 0.2f;      // outliner vs. rest
    f32 m_rightSplit = 0.78f;    // viewport vs. inspector

    Slate::Rect m_viewportRect{};
    TextureHandle m_viewport = 0;
    TextureHandle m_iconPlay = 0;
    TextureHandle m_iconPause = 0;
    TextureHandle m_iconStop = 0;
};

}  // namespace Luma
