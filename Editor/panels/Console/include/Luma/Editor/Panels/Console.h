#pragma once

#include "PanelContext.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

// Console panel — placeholder that uses the mono font. The real Console
// (5.5) will subscribe to the Log sink + filter by severity/category.

namespace Luma::Editor::Panels {

class ConsolePanel {
public:
    void Draw(Slate::Context& ui, const Slate::Rect& body, PanelContext& ctx);
};

}  // namespace Luma::Editor::Panels
