#pragma once

#include <cstdint>

#include "Luma/Math/Math.h"
#include "Luma/Material/Material.h"
#include "Luma/Material/MaterialExpression.h"

// The Material Graph schema — Luma's port of UE5.8's UMaterialGraphSchema
// (Engine/Source/Editor/UnrealEd/Classes/MaterialGraph/MaterialGraphSchema.h).
//
// The schema owns the rules that are engine-wide rather than editor-local:
//   - pin categories / sub-categories (UE PC_* / PSC_*),
//   - pin compatibility (UE ArePinsCompatible_Internal -> CanConnectMaterialValueTypes),
//   - connection validation with loop detection (UE CanCreateConnection ->
//     ConnectionCausesLoop),
//   - pin colors (UE GetPinTypeColor),
//   - the categorized expression palette (UE GetPaletteActions /
//     MaterialExpressionClasses).
// The editor panel consults this schema instead of re-implementing the rules.

namespace Luma::Material {

// UE UMaterialGraphSchema::PC_Mask / PC_Required / PC_Optional /
// PC_MaterialInput (PC_Exec is not part of Luma's graph yet).
enum class PinCategory : u8 {
    Mask,          // component-mask pin (PSC_Red..PSC_RGBA)
    Required,      // required input pin (must connect)
    Optional,      // optional input pin
    MaterialInput, // material property input on the root node
};

// UE UMaterialGraphSchema::PSC_* — sub-category for mask pins.
enum class PinSubCategory : u8 {
    None,
    Red,
    Green,
    Blue,
    Alpha,
    RG,
    RGB,
    RGBA,
};

class MaterialGraphSchema {
public:
    // UE CanConnectMaterialValueTypes — pin compatibility rule.
    static bool ArePinsCompatible(MaterialValueType inputType,
                                  MaterialValueType outputType);

    // Whether `fromNode`'s output `outputIndex` may connect into `toNode`'s
    // input `inputIndex`. Mirrors UMaterialGraphSchema::CanCreateConnection:
    // type compatibility + loop detection (a connection that would make the
    // graph cyclic is rejected).
    static bool CanCreateConnection(const MaterialGraph& graph,
                                    const MaterialExpression& fromNode,
                                    i32 outputIndex,
                                    const MaterialExpression& toNode,
                                    i32 inputIndex);

    // Declared value type of a material property input (UE
    // UMaterialGraphSchema::GetMaterialIOValueType).
    static MaterialValueType GetMaterialIOValueType(MaterialProperty p);

    // UE UMaterialGraphSchema::GetPinTypeColor — RGB (0..1) for a value type.
    // The editor converts to its own Slate color; the schema stays UI-free.
    static Math::Vec3 GetPinTypeColor(MaterialValueType type);

    // Categorized expression palette (UE: GetPaletteActions, fed by
    // MaterialExpressionClasses). The editor's right-click palette renders
    // exactly this table, so new node categories appear everywhere from one
    // edit.
    struct PaletteItem {
        const char* label;
        ExpressionKind kind;
    };
    struct PaletteCategory {
        const char* label;
        const PaletteItem* items;
        int count;
    };
    static const PaletteCategory* Palette(int& outCount);

    // Pin category / sub-category helpers.
    static PinCategory GetInputPinCategory(const MaterialExpression& node,
                                           i32 inputIndex);
};

}  // namespace Luma::Material
