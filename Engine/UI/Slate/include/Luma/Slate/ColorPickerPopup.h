#pragma once

#include <functional>
#include <string_view>

#include "Luma/Slate/ColorPicker.h"
#include "Luma/Slate/Context.h"

// Floating color picker pattern built on top of ColorPicker (which itself
// stays popup-free and inline-usable). Draws a clickable swatch; clicking it
// opens a floating panel anchored beside the swatch that hosts a ColorPicker
// as its child. The panel closes on outside-click or on pressing the swatch
// again. Any color change (in the picker) fires OnColorChanged.
//
// Rendering is split in two so the floating panel is not clipped by the host
// panel's clip rect (the dock clips every panel to its bounds):
//   - DrawSwatch() draws the button and toggles the open state; call it
//     inside the panel (its Draw method).
//   - DrawPanel() draws the floating picker panel if open; call it AFTER the
//     panel clip stack is unwound (e.g. from the editor shell after the dock)
//     so the panel can overflow the host column.
//   - Draw() is the combined convenience form, for hosts that aren't clipped.

namespace Luma::Slate {

class ColorPickerPopup {
public:
    explicit ColorPickerPopup(Color initial = Color::RGB(255, 255, 255));

    // Draws the swatch button (with an optional label to its right) and
    // toggles the open state on click. Call inside a panel. Returns false
    // (the picker only edits inside DrawPanel).
    bool DrawSwatch(Context& ui, const Rect& swatchRect,
                    std::string_view label = {});

    // Draws the floating picker panel if open. Call after the clip stack is
    // unwound. Returns true if the color changed this frame.
    bool DrawPanel(Context& ui);

    // Convenience: DrawSwatch + DrawPanel (usable when the host panel isn't
    // clipped, e.g. top-level overlays).
    bool Draw(Context& ui, const Rect& swatchRect, std::string_view label = {});

    Color color() const { return m_picker.color(); }
    void SetColor(Color c) { m_picker.SetColor(c); }
    bool IsOpen() const { return m_open; }
    void Open() { m_open = true; }
    void Close() { m_open = false; }

    // Invoked whenever the color changes through the picker.
    std::function<void(const Color&)> OnColorChanged;

private:
    ColorPicker m_picker;
    bool m_open = false;
    Rect m_swatchRect{};  // anchor recorded by the last DrawSwatch
};

}  // namespace Luma::Slate
