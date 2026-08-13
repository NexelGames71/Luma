#include "Luma/Editor/Panels/WorldOutliner.h"

#include <string>

#include "Luma/Scene/Components.h"

namespace Luma::Editor::Panels {

using Slate::Align;
using Slate::Rect;

void WorldOutlinerPanel::Draw(Slate::Context& ui, const Slate::Rect& body,
                              PanelContext& ctx) {
    Slate::Theme& t = ui.theme();

    if (ui.Button(Slate::Context::ID("outliner.add"),
                  {body.Right() - 52, body.y + 4, 44, 22}, "+ Add")) {
        if (ctx.onAddEntity) ctx.onAddEntity();
        ++m_nextNumber;
    }

    auto view = ctx.scene->Registry().view<const NameComponent>();
    if (view.begin() == view.end()) {
        ui.LabelIn({body.x, body.y + 8, body.w, 22}, "  (no entities)",
                   t.textDim);
        return;
    }
    f32 y = body.y + 32.0f;
    for (Entity e : view) {
        const std::string& name = view.get<const NameComponent>(e).name;
        Rect row{body.x + 4, y, body.w - 8, 24};
        if (ui.Selectable(Slate::Context::ID(name.c_str()), row, name,
                          e == *ctx.selected)) {
            *ctx.selected = e;
        }
        y += 26.0f;
    }
}

}  // namespace Luma::Editor::Panels
