#pragma once

#include <functional>
#include <string>

#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

// Luma Slate compound widget: a full color picker (saturation/value gradient
// square, hue strip, RGB + HSV spin boxes, hex field, preview swatch). Like
// the other compound widgets (TreeView/ListView style panels), it is an
// immediate-mode class that owns its state across frames and is drawn into a
// caller-provided rect — usable inline in any panel. No popup/floating
// behavior lives here; see ColorPickerPopup for the "click a swatch to open
// a floating picker" wrapper.
//
// All controls write into one shared Color; any change (drag, spin-box
// commit, hex commit) fires OnColorChanged so callers can sync scene data.
// Text fields commit on Enter or on outside-click; invalid input reverts to
// the previous color.

namespace Luma::Slate {

class ColorPicker {
public:
    explicit ColorPicker(Color initial = Color::RGB(255, 255, 255));

    // Draws the picker into `rect` and returns true if the color changed
    // this frame (SV/hue drag, spin-box commit, or hex commit).
    bool Draw(Context& ui, const Rect& rect);

    Color color() const { return m_color; }
    void SetColor(Color c) { m_color = c; }

    // Invoked whenever the color changes through any control.
    std::function<void(const Color&)> OnColorChanged;

    // Fixed natural layout height (SV square + two spin-box rows + padding).
    static constexpr f32 kHeight = 204.0f;

private:
    bool DrawSVSquare(Context& ui, const Rect& rect);
    bool DrawHueStrip(Context& ui, const Rect& rect);
    void DrawSwatch(Context& ui, const Rect& rect);
    bool DrawHexField(Context& ui, const Rect& rect);
    bool DrawSpinRows(Context& ui, const Rect& rect);

    // Number/hex field commit helpers (parse + clamp + apply to m_color).
    bool CommitRgb(int channel, const std::string& text);
    bool CommitHsv(int channel, const std::string& text);
    bool CommitHex();

    // Formats the current color for a spin-box / hex buffer.
    static std::string FormatRgb(int channel, Color c);
    static std::string FormatHsv(int channel, f32 h, f32 s, f32 v);
    static std::string FormatHex(Color c);

    // Per-instance id base so multiple pickers don't collide in Context's
    // focus/active tables.
    u64 IdBase() const {
        return reinterpret_cast<u64>(this) ^ Context::ID("slate.colorpicker");
    }

    Color m_color;

    // Derived HSV (refreshed from m_color at the top of each Draw so the
    // gradient square + hue strip follow external SetColor calls).
    f32 m_hue = 0.0f;
    f32 m_sat = 1.0f;
    f32 m_val = 1.0f;

    // The color the current HSV values were derived from. When m_color
    // diverges (external SetColor / scene load) the HSV state re-derives;
    // drags update it in place so hue survives a trip through black.
    Color m_hsvSource;

    // Drag ownership for the two draggable surfaces.
    bool m_svDragging = false;
    bool m_hueDragging = false;

    // Text-field edit buffers + in-progress flags. Buffers only refresh from
    // the color while their field is not being edited, so typing is never
    // clobbered; commits re-sync them.
    std::string m_rgb[3];
    std::string m_hsv[3];
    std::string m_hex;
    bool m_editingRgb[3] = {};
    bool m_editingHsv[3] = {};
    bool m_editingHex = false;
};

}  // namespace Luma::Slate
