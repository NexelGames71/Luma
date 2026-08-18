#pragma once

#include <string_view>
#include <vector>

#include "Luma/Material/MaterialExpression.h"
#include "Luma/Material/MaterialValueType.h"

// The data-driven material node registry — Luma's single source of truth for
// the node palette. Every node is one entry: its display name, palette
// category, pin layout, and inline property fields. The editor palette, the
// node factory (MakeExpression) and the node drawing all derive from this
// table, so a node is defined exactly once and stays consistent everywhere.
//
// Adding a node = appending one MaterialNodeSpec to MaterialNodeSpecs().
// The registry is deliberately EMPTY until nodes are added back one by one.
//
// Engine code that needs per-kind behavior NOT expressible as data (the
// GLSL compiler) still switches on ExpressionKind — the table is the editor
// surface, the compiler is the runtime surface.

namespace Luma::Material {

// Which MaterialExpression field a displayed property edits. Each entry in
// a node's property list binds a label to one of these.
enum class NodePropField : u8 {
    None,
    Bool,          // bool constBool (checkbox)
    Bool2,         // bool constBool2 (checkbox)
    Int,           // i32 constInt (integer drag)
    Scalar,        // f32 constScalar (drag)
    Vec2,          // Math::Vec2 constVec2 (two drags)
    Color,         // Math::Vec3 constVec3 (color swatch)
    Vec4,          // Math::Vec4 constVec4 (four drags)
    String,        // std::string paramName (text field)
    Dropdown,      // i32 coordinateIndex (dropdown, items from `items`)
    Texture,       // AssetId texture (asset-registry dropdown)
    Mask,          // maskR/maskG/maskB/maskA (channel checkboxes)
    VectorDim,     // i32 vectorDim (dropdown; updates the output arity)
    VectorComponents,  // Math::Vec4 vectorValue (dimension-driven split row)
    LerpAlpha,     // f32 lerpAlpha
    ClampMin,      // f32 clampMin
    ClampMax,      // f32 clampMax
    RotatorSpeed,  // f32 rotatorSpeed
    RotatorCenter, // Math::Vec2 rotatorCenter
    Exponent,      // f32 exponent
    BaseReflect,   // f32 baseReflectFraction
    Radius,        // f32 radius
    Hardness,      // f32 hardness
    Fraction,      // f32 fraction
    NoiseScale,    // f32 noiseScale
};

// One inline property field drawn below a node's pins.
struct NodePropertySpec {
    std::string_view name;   // row label, e.g. "Value", "Min", "Name"
    NodePropField field = NodePropField::None;
    std::string_view items;  // Dropdown options, ';'-separated ("UV0;UV1;...")
    f32 defaultValue = 0.0f; // applied at creation (Scalar / Int / Bool fields)
};

// One pin of a node.
struct NodePinSpec {
    std::string_view name;
    MaterialValueType type = MCT_None;
    bool output = false;  // true = output pin, false = input pin
};

// The full definition of one node kind.
struct MaterialNodeSpec {
    ExpressionKind kind;
    std::string_view name;      // display title + palette label
    std::string_view category;  // palette category ("Constants", "Math", ...)
    std::vector<NodePinSpec> pins;   // in declared order
    std::vector<NodePropertySpec> props;  // inline fields, in display order
};

// The node registry — currently EMPTY. Nodes are added back one at a time.
const std::vector<MaterialNodeSpec>& MaterialNodeSpecs();

// Lookup helpers (nullptr when the kind has no spec yet).
const MaterialNodeSpec* MaterialNodeSpecFor(ExpressionKind kind);
// Number of inline property rows for a kind (0 when unspecified).
int MaterialNodePropCount(ExpressionKind kind);

}  // namespace Luma::Material
