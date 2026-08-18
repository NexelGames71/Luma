#pragma once

#include <string>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

// Luma::Slate::GraphNodeWidget - a reusable, data-driven node-chrome renderer
// for node-graph editors (material graphs, blueprints, tool graphs, ...).
//
// The widget draws the UE-style node shell - body, header, input/output pin
// circles, pin labels, selection border - from a plain Spec. The editor that
// owns the graph supplies its own data (pin colors from its schema, title,
// selection) and draws the node's custom body content (inline fields,
// thumbnails, ...) itself into the space below the pins. Because the widget
// is stateless and canvas-agnostic, every graph-based editor can share it:
// the caller transforms its own world coordinates to a screen-space node
// rect and passes the title/pin-row heights in the same (screen) units.
//
// Layout model (all metrics in screen px; the caller scales world metrics by
// its zoom):
//   title bar  : titleH
//   pin band   : max(inputs, outputs) * pinRowH, pins centered on each row
//   body       : caller-drawn below the pin band (BodyTop() gives the start)
namespace Luma::Slate {

class GraphNodeWidget {
public:
    // Default layout metrics (world units; multiply by the editor's zoom).
    static constexpr f32 kTitleH = 24.0f;
    static constexpr f32 kPinRowH = 18.0f;
    static constexpr f32 kBodyRowH = 22.0f;
    static constexpr f32 kPinR = 5.0f;     // pin radius (screen px)
    static constexpr f32 kPinHitR = 10.0f; // pin hit radius (screen px)

    // One pin: a label plus its wire color (from the owning schema).
    struct Pin {
        std::string label;
        Color color = Color::RGB(140, 140, 150);
    };

    // Everything the widget needs to draw one node. Blueprint/tool editors
    // color their node categories through `headerTint`; material editors can
    // leave it empty and rely on pin colors + selection.
    struct Spec {
        std::string title;
        Color headerTint{};       // category tint blended into the header
        std::vector<Pin> inputs;  // left edge
        std::vector<Pin> outputs; // right edge
        bool selected = false;
        bool centerTitle = false;  // UE central nodes center their title
        // Renders pin labels with the regular body font (larger + easier to
        // read) instead of the small font. Used by property-heavy nodes like
        // the Material Output node.
        bool largeLabels = false;
        // Draws input-pin labels INSIDE the node (right of the pin) instead
        // of outside the left edge. Used by the Material Output node, whose
        // property names belong in the body beside their value fields.
        bool inputLabelsInside = false;
        // Optional per-pin physical row (which pin-band row each input pin
        // sits on). Lets an editor insert non-pin rows (section headers,
        // dropdown rows) between pins while keeping them aligned. When empty,
        // pin i sits on row i.
        std::vector<int> inputRowMap;
    };

    // Draws the node chrome (body, header, pins + labels, selection border)
    // into the screen-space rect `node`. `titleH`/`pinRowH` are the title
    // bar and pin-band row heights in screen px (world metric * zoom).
    // Returns `node`.
    static Rect Draw(Context& ui, const Rect& node, const Spec& spec,
                     f32 titleH, f32 pinRowH);

    // Screen-space pin centers - shared by wire drawing and hit tests so
    // rendering and input always agree on the layout.
    static Vec2 InputPin(const Rect& node, f32 titleH, f32 pinRowH,
                         int index);
    static Vec2 OutputPin(const Rect& node, f32 titleH, f32 pinRowH,
                          int index);
    // True when the cursor is within the pin's hit radius.
    static bool HitPin(Vec2 pin, Vec2 screen);

    // Screen-space y where the node's body content starts (below the title
    // bar and the `pinRows`-tall pin band).
    static f32 BodyTop(const Rect& node, f32 titleH, f32 pinRowH,
                       int pinRows);
    // Total node size (in the units `titleH`/`pinRowH`/`bodyH` are given
    // in) for the common title + pin band + body layout.
    static f32 Height(f32 titleH, f32 pinRowH, int pinRows, f32 bodyH);
};

}  // namespace Luma::Slate
