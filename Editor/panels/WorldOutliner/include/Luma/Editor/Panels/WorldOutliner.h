#pragma once

#include "PanelContext.h"
#include "Luma/Scene/Scene.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

// World Outliner panel — lists every named entity in the Scene, drives
// selection (which feeds the Inspector + the gizmo via the shared
// PanelContext). Pure UI: owns no scene state.

namespace Luma::Editor::Panels {

class WorldOutlinerPanel {
public:
    void Draw(Slate::Context& ui, const Slate::Rect& body, PanelContext& ctx);

    // Clears the cached selection hint (called after the scene is reloaded
    // so the panel doesn't reference an entity that no longer exists).
    void ClearSelection() {}

private:
    int m_nextNumber = 1;
};

}  // namespace Luma::Editor::Panels
