#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Luma/Asset/AssetId.h"
#include "Luma/Math/Math.h"
#include "Luma/Material/MaterialTypes.h"

// The Material node-graph object model — Luma's counterpart to Unreal's
// UMaterialExpression / FExpressionInput / FExpressionOutput trio (see
// UE5.8 Engine/Source/Runtime/Engine/Public/Materials/MaterialExpression.h
// and MaterialExpressionIO.h).
//
// The model is deliberately engine-owned (runtime, no editor dependency):
// the editor window edits this graph, the .lmat serializer persists it, and
// the MaterialCompiler turns it into shader code at runtime. Connections are
// stored on each input as (source expression id, output index) — exactly how
// UE's FExpressionInput points at a UMaterialExpression + OutputIndex.

namespace Luma::Material {

class MaterialGraph;

// A typed connection on a node input: points at another expression's output
// pin. Mirrors UE's FExpressionInput (Expression + OutputIndex + InputName +
// expected type). `expected` is the declared pin type of this input; the
// compiler validates the source output matches (see
// MaterialGraphSchema::ArePinsCompatible).
struct ExpressionInput {
    ExpressionId expression = kInvalidExpressionId;  // source expression
    i32 outputIndex = 0;      // which output pin of the source expression
    std::string name;         // pin label (mirrors ExpressionOutput::name)
    MaterialValueType expected = MCT_None;  // declared type of this input

    bool IsConnected() const { return expression != kInvalidExpressionId; }
    void Disconnect() { expression = kInvalidExpressionId; }
};

// One output pin descriptor. Mirrors UE's FExpressionOutput (OutputName +
// type).
struct ExpressionOutput {
    std::string name;  // e.g. "RGBA", "R", "RGB"
    MaterialValueType type = MCT_None;
};

// A node in the material graph. Kind-driven (no per-class hierarchy needed at
// this stage): each kind declares its inputs/outputs and carries the params
// that kind needs. The compiler switches on `kind` to emit GLSL.
//
// The type-resolution methods mirror UMaterialGraphNode_Base's
// GetInputValueType / GetOutputValueType: dynamic (MCT_Unknown) pins resolve
// to a concrete type by walking the connected graph.
struct MaterialExpression {
    ExpressionId id = kInvalidExpressionId;  // stable across save/load
    ExpressionKind kind = ExpressionKind::Constant;
    std::string title;  // display name; defaulted from kind when empty

    // Canvas position (editor-facing; harmless at runtime).
    f32 x = 0.0f;
    f32 y = 0.0f;

    // --- Kind parameters (flat across the node set; per-kind usage below) ---
    bool constBool = false;                   // Boolean constant / first bool prop
    bool constBool2 = false;                  // second bool property of a node
    i32 constInt = 0;                         // Integer constant / int property
    i32 vectorDim = 3;                        // Vector: component count (2..4)
    Math::Vec4 vectorValue{1.0f, 1.0f, 1.0f, 1.0f};  // Vector: components
    f32 constScalar = 1.0f;                   // Constant / ScalarParameter default
    Math::Vec2 constVec2{1.0f, 1.0f};         // Constant2
    Math::Vec3 constVec3{1.0f, 1.0f, 1.0f};   // Constant3 / VectorParameter default
    Math::Vec4 constVec4{1.0f, 1.0f, 1.0f, 1.0f};  // Constant4
    std::string paramName;                    // ScalarParameter / VectorParameter
    AssetId texture;                          // TextureSample
    i32 coordinateIndex = 0;                  // TextureCoordinate (UV set)
    f32 lerpAlpha = 0.5f;                     // Lerp (default when alpha isn't connected)
    f32 clampMin = 0.0f;                      // Clamp (default when min isn't connected)
    f32 clampMax = 1.0f;                      // Clamp (default when max isn't connected)
    f32 rotatorSpeed = 0.5f;                  // Rotator (default when Speed isn't connected)
    Math::Vec2 rotatorCenter{0.5f, 0.5f};     // Rotator rotation center
    f32 exponent = 1.0f;                      // Fresnel (default when Exponent isn't connected)
    f32 baseReflectFraction = 0.04f;          // Fresnel (default when BaseReflectFraction isn't connected)
    f32 radius = 0.5f;                        // SphereMask (default when Radius isn't connected)
    f32 hardness = 0.0f;                      // SphereMask (default when Hardness isn't connected)
    f32 fraction = 1.0f;                      // Desaturation amount
    f32 noiseScale = 1.0f;                    // VectorNoise scale
    bool maskR = true;                        // ComponentMask channel selection
    bool maskG = true;
    bool maskB = true;
    bool maskA = false;

    // --- Pins ---
    // Inputs are populated from `kind` when the node is created (Add: 2,
    // TextureSample: 1 UV, ...). The compiler reads them by index.
    std::vector<ExpressionInput> inputs;
    std::vector<ExpressionOutput> outputs;

    bool IsComment() const { return kind == ExpressionKind::Comment; }
    const ExpressionInput* Input(i32 index) const;
    ExpressionInput* Input(i32 index);

    // --- Node-base type resolution (UE UMaterialGraphNode_Base) -------------
    // Declared (pin-descriptor) value type of an input.
    MaterialValueType GetInputValueType(i32 index) const;
    // Declared value type of an output.
    MaterialValueType GetOutputValueType(i32 index) const;
    // Resolves an output's CONCRETE type by walking the graph: dynamic
    // (MCT_Unknown) pins mirror the first connected input's type — e.g.
    // Abs(vec3) outputs vec3, Add(vec3, 1.0) outputs vec3. Falls back to the
    // declared type when nothing is connected.
    MaterialValueType ResolveOutputType(const MaterialGraph& graph,
                                        i32 outputIndex) const;
};

// Node factory: returns a new expression of `kind` with the standard pin
// layout and a default title. Id is assigned by the caller-provided counter.
MaterialExpression MakeExpression(ExpressionKind kind, ExpressionId id);

// Default display title for a kind (used when the user hasn't renamed).
std::string_view DefaultExpressionTitle(ExpressionKind kind);
// Alias used by editor UI for the node's display name (same mapping).
inline std::string_view ExpressionKindToString(ExpressionKind kind) {
    return DefaultExpressionTitle(kind);
}

}  // namespace Luma::Material
