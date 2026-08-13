#pragma once

#include "PanelContext.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

// Inspector panel — shows the selected entity's components with editable
// fields. Reflection-driven: every reflected component (Name, Transform,
// MeshRenderer, Camera, Light, Environment) auto-builds its rows from its
// TypeInfo. The + Component popup lets the user add missing components.

namespace Luma::Editor::Panels {

class InspectorPanel {
public:
    void Draw(Slate::Context& ui, const Slate::Rect& body, PanelContext& ctx);

    // Closes any open popup (e.g. after the selection changes).
    void ClosePopups() { m_showAddMenu = false; }

private:
    bool m_showAddMenu = false;
};

}  // namespace Luma::Editor::Panels
