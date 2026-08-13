#pragma once

#include "PanelContext.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

// Viewport panel — hosts the 3D scene view (texture drawn from a SceneView
// the editor produces), the camera controls (right-drag orbit, scroll zoom),
// and the translate gizmo for the selected entity. Pure UI; the panel reads
// scene state from PanelContext and reports back which rect it occupies via
// Rect() for the renderer to know where to draw the 3D target.

namespace Luma::Editor::Panels {

class ViewportPanel {
public:
    void Draw(Slate::Context& ui, const Slate::Rect& body, PanelContext& ctx);

    // The rect the viewport most recently occupied — the EditorScreen reads
    // this to know where to render the 3D scene into.
    Slate::Rect Rect() const { return m_rect; }

    // The EditorScreen calls this each frame after rendering the 3D view so
    // the panel can display the texture.
    void SetTexture(Luma::TextureHandle tex) { m_texture = tex; }

private:
    Slate::Rect m_rect{};
    Luma::TextureHandle m_texture = 0;
};

}  // namespace Luma::Editor::Panels
