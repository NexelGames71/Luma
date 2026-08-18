#include "Luma/Slate/GraphNodeWidget.h"

namespace Luma::Slate {

Rect GraphNodeWidget::Draw(Context& ui, const Rect& node, const Spec& spec,
                           f32 titleH, f32 pinRowH) {
    Theme& t = ui.theme();
    const f32 r = t.radius.sm;

    // Body: a dark, clearly translucent surface with a subtle metallic sheen
    // (lighter at the top, falling to near-black at the bottom) so the canvas
    // grid shows through and the node reads as tinted glass over the graph.
    ui.drawList().AddRectFilledGradientRounded(
        node, Color::RGBA(42, 47, 55, 88), Color::RGBA(12, 14, 18, 118),
        {r, r, r, r});

    // Header bar (rounded top corners): a vertical gradient from the node's
    // category tint (strong at the top, fading down) into the dark body, so
    // every node kind carries its own color. Selection pushes the tint
    // harder; without a tint the gradient stays neutral metallic.
    Rect header{node.x, node.y, node.w, titleH};
    Color hTop = Color::RGBA(50, 54, 62, 235);
    Color hBot = Color::RGBA(22, 24, 29, 245);
    Color tint = spec.headerTint.a > 0 ? spec.headerTint : t.accent;
    f32 tintAmt = spec.headerTint.a > 0 ? (spec.selected ? 0.85f : 0.55f)
                                        : (spec.selected ? 0.35f : 0.0f);
    Color top = tintAmt > 0.0f ? Mix(hTop, tint, tintAmt) : hTop;
    Color bot = tintAmt > 0.0f ? Mix(hBot, tint, tintAmt * 0.45f) : hBot;
    ui.drawList().AddRectFilledGradientRounded(header, top, bot,
                                               {r, r, 0.0f, 0.0f});
    ui.drawList().AddLine({header.x, header.Bottom()},
                          {header.Right(), header.Bottom()}, t.outline, 1.0f);

    if (!spec.title.empty()) {
        if (spec.centerTitle) {
            ui.LabelIn({header.x + 4.0f, header.y, header.w - 8.0f, header.h},
                       spec.title, t.text, Align::Center);
        } else {
            ui.LabelIn({header.x + 8.0f, header.y, header.w - 16.0f, header.h},
                       spec.title, t.text, Align::Left);
        }
    }

    // Pin labels: regular body font for property-heavy nodes (largeLabels),
    // small font for compact graph nodes. Both are fixed-size — zooming the
    // canvas never scales fonts, so labels stay readable at any zoom.
    const Font& labelFont = spec.largeLabels ? ui.font() : ui.smallFont();
    const f32 labelDy = labelFont.LineHeight() * 0.5f;

    // Input pins on the left edge. By default the labels sit just outside
    // the node (right-aligned against the pin); the Material Output node
    // instead draws its property names inside the body, left-aligned after
    // the pin, next to its value fields. `inputRowMap` lets an editor insert
    // non-pin rows (section headers / dropdowns) between pins.
    for (usize i = 0; i < spec.inputs.size(); ++i) {
        const int physRow =
            !spec.inputRowMap.empty() && i < spec.inputRowMap.size()
                ? spec.inputRowMap[i]
                : static_cast<int>(i);
        Vec2 pos = InputPin(node, titleH, pinRowH, physRow);
        ui.drawList().AddCircleFilled(pos, kPinR, Color::RGB(15, 15, 18), 16);
        ui.drawList().AddCircleFilled(pos, kPinR - 1.0f, spec.inputs[i].color,
                                      16);
        if (!spec.inputs[i].label.empty()) {
            f32 tw = labelFont.CalcTextSize(spec.inputs[i].label).x;
            Vec2 label = spec.inputLabelsInside
                             ? Vec2{pos.x + kPinR + 5.0f, pos.y - labelDy}
                             : Vec2{pos.x - kPinR - 5.0f - tw,
                                    pos.y - labelDy};
            ui.drawList().AddText(labelFont, label, spec.inputs[i].label,
                                  t.text);
        }
    }
    // Output pins on the right edge (labels right-aligned beside the pin).
    for (usize i = 0; i < spec.outputs.size(); ++i) {
        Vec2 pos = OutputPin(node, titleH, pinRowH, static_cast<int>(i));
        ui.drawList().AddCircleFilled(pos, kPinR, Color::RGB(15, 15, 18), 16);
        ui.drawList().AddCircleFilled(pos, kPinR - 1.0f, spec.outputs[i].color,
                                      16);
        if (!spec.outputs[i].label.empty()) {
            Vec2 label{pos.x + kPinR + 5.0f, pos.y - labelDy};
            ui.drawList().AddText(labelFont, label, spec.outputs[i].label,
                                  t.text);
        }
    }

    // Selection border: accent + thicker when selected, hairline otherwise.
    if (spec.selected) {
        ui.drawList().AddRect(node, t.accent, 2.0f, r);
    } else {
        ui.drawList().AddRect(node, t.outline, 1.0f, r);
    }
    return node;
}

Vec2 GraphNodeWidget::InputPin(const Rect& node, f32 titleH, f32 pinRowH,
                               int index) {
    return {node.x,
            node.y + titleH +
                (static_cast<f32>(index) + 0.5f) * pinRowH};
}

Vec2 GraphNodeWidget::OutputPin(const Rect& node, f32 titleH, f32 pinRowH,
                                int index) {
    return {node.Right(),
            node.y + titleH +
                (static_cast<f32>(index) + 0.5f) * pinRowH};
}

bool GraphNodeWidget::HitPin(Vec2 pin, Vec2 screen) {
    f32 dx = pin.x - screen.x;
    f32 dy = pin.y - screen.y;
    return dx * dx + dy * dy <= kPinHitR * kPinHitR;
}

f32 GraphNodeWidget::BodyTop(const Rect& node, f32 titleH, f32 pinRowH,
                             int pinRows) {
    return node.y + titleH + static_cast<f32>(pinRows) * pinRowH;
}

f32 GraphNodeWidget::Height(f32 titleH, f32 pinRowH, int pinRows, f32 bodyH) {
    return titleH + static_cast<f32>(pinRows) * pinRowH + bodyH;
}

}  // namespace Luma::Slate
