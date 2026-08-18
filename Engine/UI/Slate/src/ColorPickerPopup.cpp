#include "Luma/Slate/ColorPickerPopup.h"

namespace Luma::Slate {

ColorPickerPopup::ColorPickerPopup(Color initial) : m_picker(initial) {}

bool ColorPickerPopup::DrawSwatch(Context& ui, const Rect& swatchRect,
                                  std::string_view label) {
    Theme& t = ui.theme();
    m_swatchRect = swatchRect;

    // Swatch button: opens/closes the floating panel.
    bool hovered = swatchRect.Contains(ui.mouse());
    if (hovered) ui.RequestCursor(CursorShape::Hand);
    if (hovered && ui.mousePressed(0)) m_open = !m_open;

    ui.drawList().AddRectShadow(swatchRect, t.radius.md, 0.15f, 4.0f);
    Color border = hovered ? t.accent : t.outline;
    ui.drawList().AddRectFilledRounded(swatchRect, border, t.radius.md);
    ui.drawList().AddRectFilledRounded(
        swatchRect.Inset(t.border.hairline, t.border.hairline),
        m_picker.color(),
        std::max(0.0f, t.radius.md - t.border.hairline));

    if (!label.empty()) {
        ui.LabelIn({swatchRect.Right() + 6.0f, swatchRect.y,
                    ui.uiFont().Measure(label).x, swatchRect.h},
                   label, hovered ? t.text : t.textDim);
    }
    return false;
}

bool ColorPickerPopup::DrawPanel(Context& ui) {
    if (!m_open) return false;
    Theme& t = ui.theme();

    // Anchor beside the swatch: right when there's room, otherwise left.
    // Vertically below the swatch, flipped above when near the bottom.
    const f32 pickerW = 250.0f;
    const f32 panelW = pickerW + 12.0f;  // backdrop inset on each side
    f32 px = m_swatchRect.Right() + 8.0f;
    if (px + panelW > ui.displayWidth()) {
        px = m_swatchRect.x - 8.0f - panelW;
        if (px < 0.0f) px = 0.0f;
    }
    f32 py = m_swatchRect.Bottom() + 4.0f;
    if (py + ColorPicker::kHeight + 12.0f > ui.displayHeight()) {
        py = m_swatchRect.y - ColorPicker::kHeight - 12.0f - 4.0f;
        if (py < 0.0f) py = 0.0f;
    }
    Rect panel{px, py, pickerW, ColorPicker::kHeight};

    // Backdrop card so the picker reads as a floating surface.
    ui.PanelRoundedBordered(panel.Inset(-6.0f, -6.0f), t.surface4, t.accent,
                            t.radius.md, t.border.thick);
    bool changed = m_picker.Draw(ui, panel);

    // Outside-click (or pressing the swatch again) closes the panel.
    if (ui.mousePressed(0) && !panel.Contains(ui.mouse()) &&
        !m_swatchRect.Contains(ui.mouse())) {
        m_open = false;
    }

    if (changed && OnColorChanged) OnColorChanged(m_picker.color());
    return changed;
}

bool ColorPickerPopup::Draw(Context& ui, const Rect& swatchRect,
                            std::string_view label) {
    DrawSwatch(ui, swatchRect, label);
    return DrawPanel(ui);
}

}  // namespace Luma::Slate
