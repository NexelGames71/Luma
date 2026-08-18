#pragma once

#include "PanelContext.h"
#include "Luma/Slate/ColorPickerPopup.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

// Inspector panel — shows the selected entity's components with editable
// fields. Reflection-driven: every reflected component (Name, Transform,
// MeshRenderer, Camera, Light, Environment) auto-builds its rows from its
// TypeInfo. The + Component popup lets the user add missing components.
//
// Color properties (albedo, light color, sky tint, ground color) are compact
// swatch strips: clicking one opens a floating ColorPicker panel (see
// ColorPickerPopup). The panels are drawn by DrawFloatingPickers, which the
// editor shell calls AFTER the dock so they aren't clipped to this column.

namespace Luma::Editor::Panels {

class InspectorPanel {
public:
    void Draw(Slate::Context& ui, const Slate::Rect& body, PanelContext& ctx);

    // Draws any open floating color picker panels. Call after the dock's clip
    // stack is unwound so the panels can overflow this column.
    void DrawFloatingPickers(Slate::Context& ui, PanelContext& ctx);

    // Closes any open popup (e.g. after the selection changes).
    void ClosePopups();

private:
    bool m_showAddMenu = false;

    // Floating color pickers for the color property rows. State lives here so
    // each picker's edit buffers + open state persist across frames.
    Slate::ColorPickerPopup m_albedoPicker;
    Slate::ColorPickerPopup m_lightColorPicker;
    Slate::ColorPickerPopup m_skyTintPicker;
    Slate::ColorPickerPopup m_groundColorPicker;

    // Last entity the inspector drew, used to close popups when the selection
    // changes (the pickers target the selected entity's components).
    Entity m_lastSelected = kNullEntity;

    // Vertical scroll offset of the component content (px). Reset to the top
    // when the selection changes so a new entity starts scrolled at zero.
    f32 m_scroll = 0.0f;
};

}  // namespace Luma::Editor::Panels
