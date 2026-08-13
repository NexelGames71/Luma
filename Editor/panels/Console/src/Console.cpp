#include "Luma/Editor/Panels/Console.h"

namespace Luma::Editor::Panels {

using Slate::Align;
using Slate::Rect;

void ConsolePanel::Draw(Slate::Context& ui, const Slate::Rect& body,
                        PanelContext& /*ctx*/) {
    // Console text uses the mono font (typography.type.mono -> m_monoFont).
    ui.LabelInMono({body.x + 12, body.y + 8, body.w - 20, 22},
                   "Luma Editor ready.", ui.theme().textDim);
}

}  // namespace Luma::Editor::Panels
