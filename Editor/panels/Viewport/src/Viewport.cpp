#include "Luma/Editor/Panels/Viewport.h"

#include <algorithm>
#include <cmath>

#include "Luma/Gizmo/TranslateGizmo.h"
#include "Luma/Math/Math.h"
#include "Luma/Scene/Components.h"

namespace Luma::Editor::Panels {

using Slate::Align;
using Slate::Rect;

void ViewportPanel::Draw(Slate::Context& ui, const Slate::Rect& body,
                         PanelContext& ctx) {
    Slate::Theme& t = ui.theme();
    m_rect = body;

    if (m_texture) {
        ui.Image(m_texture, body);
    } else {
        ui.Panel(body, t.surface0);
        ui.LabelIn(body, "3D Viewport", t.textDim, Align::Center);
    }

    // Gizmo drag (left mouse) takes priority over camera when a selection exists.
    bool gizmoActive = false;
    if (ctx.scene->IsValid(*ctx.selected) &&
        ctx.scene->Registry().all_of<TransformComponent>(*ctx.selected)) {
        auto& tf = ctx.scene->Registry().get<TransformComponent>(*ctx.selected);
        GizmoInput in;
        in.view = *ctx.view;
        in.proj = Luma::Math::Perspective(ctx.fovY, body.w / body.h, ctx.nearZ,
                                          ctx.farZ);
        in.viewportX = body.x;
        in.viewportY = body.y;
        in.viewportW = body.w;
        in.viewportH = body.h;
        in.mouseX = ui.mouse().x;
        in.mouseY = ui.mouse().y;
        in.scale = *ctx.gizmoScale;
        in.leftDown = body.Contains(ui.mouse()) || ctx.gizmo->Dragging();
        in.leftDown = in.leftDown && ui.isMouseDown(0);
        in.leftPressed = body.Contains(ui.mouse()) && ui.mousePressed(0);
        Luma::Math::Vec3 newPos;
        if (ctx.gizmo->Update(tf.position, in, newPos)) {
            tf.position = newPos;
        }
        gizmoActive = ctx.gizmo->Dragging();
    }

    if (gizmoActive || !body.Contains(ui.mouse())) return;
    if (ui.isMouseDown(1)) {
        Luma::Slate::Vec2 d = ui.mouseDelta();
        *ctx.camYaw -= d.x * 0.01f;
        *ctx.camPitch += d.y * 0.01f;
        *ctx.camPitch = std::clamp(*ctx.camPitch, -1.45f, 1.45f);
    }
    f32 scroll = ui.scrollDelta();
    if (scroll != 0.0f) {
        *ctx.camDistance *= std::pow(0.9f, scroll);
        *ctx.camDistance = std::clamp(*ctx.camDistance, 2.0f, 60.0f);
    }
}

}  // namespace Luma::Editor::Panels
