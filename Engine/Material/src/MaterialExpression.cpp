#include "Luma/Material/MaterialExpression.h"

#include "Luma/Material/MaterialGraph.h"
#include "Luma/Material/MaterialNodeSpec.h"

namespace Luma::Material {

const ExpressionInput* MaterialExpression::Input(i32 index) const {
    if (index < 0 || static_cast<usize>(index) >= inputs.size()) return nullptr;
    return &inputs[static_cast<usize>(index)];
}

ExpressionInput* MaterialExpression::Input(i32 index) {
    if (index < 0 || static_cast<usize>(index) >= inputs.size()) return nullptr;
    return &inputs[static_cast<usize>(index)];
}

// Node factory. Pin layout comes entirely from the data-driven node registry
// (MaterialNodeSpecs): each registered kind declares its input/output pins
// and they are created here verbatim. Kinds without a spec yet produce a
// bare node (title only, no pins) so old .lmat files still load.
MaterialExpression MakeExpression(ExpressionKind kind, ExpressionId id) {
    MaterialExpression e;
    e.id = id;
    e.kind = kind;
    e.title = std::string(DefaultExpressionTitle(kind));

    const MaterialNodeSpec* spec = MaterialNodeSpecFor(kind);
    if (!spec) return e;
    for (const auto& pin : spec->pins) {
        if (pin.output) {
            e.outputs.push_back(
                ExpressionOutput{std::string(pin.name), pin.type});
        } else {
            ExpressionInput in;
            in.name = std::string(pin.name);
            in.expected = pin.type;
            e.inputs.push_back(std::move(in));
        }
    }
    // Apply declared property defaults (creation-time values, e.g. the AO
    // node's 16 samples). Fields without an explicit default keep the
    // MaterialExpression struct defaults.
    for (const auto& prop : spec->props) {
        switch (prop.field) {
            case NodePropField::Scalar:
                e.constScalar = prop.defaultValue;
                break;
            case NodePropField::Int:
                e.constInt = static_cast<i32>(prop.defaultValue);
                break;
            case NodePropField::Bool:
                e.constBool = prop.defaultValue != 0.0f;
                break;
            case NodePropField::Bool2:
                e.constBool2 = prop.defaultValue != 0.0f;
                break;
            default:
                break;
        }
    }
    return e;
}

// ---------------------------------------------------------------------------
// Node-base type resolution (UE UMaterialGraphNode_Base)
// ---------------------------------------------------------------------------

MaterialValueType MaterialExpression::GetInputValueType(i32 index) const {
    const ExpressionInput* in = Input(index);
    return in ? in->expected : MCT_None;
}

MaterialValueType MaterialExpression::GetOutputValueType(i32 index) const {
    if (index < 0 || static_cast<usize>(index) >= outputs.size()) {
        return MCT_None;
    }
    return outputs[static_cast<usize>(index)].type;
}

namespace {

// Type of the first CONNECTED input of `node` (walking sources
// recursively), or `fallback` when every input is unconnected.
MaterialValueType FirstConnectedInputType(const MaterialGraph& graph,
                                          const MaterialExpression& node,
                                          MaterialValueType fallback) {
    for (const auto& in : node.inputs) {
        if (!in.IsConnected()) continue;
        const MaterialExpression* src = graph.Find(in.expression);
        if (src) {
            return src->ResolveOutputType(graph, in.outputIndex);
        }
    }
    return fallback;
}

}  // namespace

MaterialValueType MaterialExpression::ResolveOutputType(
    const MaterialGraph& graph, i32 outputIndex) const {
    switch (kind) {
        // Dynamic pass-through: mirror the first connected input.
        case ExpressionKind::Abs:
        case ExpressionKind::SquareRoot:
        case ExpressionKind::Sine:
        case ExpressionKind::Cosine:
        case ExpressionKind::Tangent:
        case ExpressionKind::Arctangent:
        case ExpressionKind::Floor:
        case ExpressionKind::Ceil:
        case ExpressionKind::Round:
        case ExpressionKind::Truncate:
        case ExpressionKind::Frac:
        case ExpressionKind::Sign:
        case ExpressionKind::OneMinus:
        case ExpressionKind::Saturate:
            return FirstConnectedInputType(graph, *this, MCT_Float1);

        // Dynamic binary: mirror the first connected operand so a chain like
        // (vec3 + vec3) * 0.5 keeps its vector type through every hop.
        case ExpressionKind::Add:
        case ExpressionKind::Subtract:
        case ExpressionKind::Multiply:
        case ExpressionKind::Divide:
        case ExpressionKind::Min:
        case ExpressionKind::Max:
        case ExpressionKind::Fmod:
        case ExpressionKind::Power:
        case ExpressionKind::Lerp:
        case ExpressionKind::InverseLerp:
        case ExpressionKind::Arctangent2:
        case ExpressionKind::Step:
        case ExpressionKind::SmoothStep:
        case ExpressionKind::Clamp:
            return FirstConnectedInputType(graph, *this, MCT_Float1);

        case ExpressionKind::ComponentMask: {
            int n = (maskR ? 1 : 0) + (maskG ? 1 : 0) +
                    (maskB ? 1 : 0) + (maskA ? 1 : 0);
            if (n >= 4) return MCT_Float4;
            if (n == 3) return MCT_Float3;
            if (n == 2) return MCT_Float2;
            return MCT_Float1;
        }

        default:
            return GetOutputValueType(outputIndex);
    }
}

std::string_view DefaultExpressionTitle(ExpressionKind kind) {
    const MaterialNodeSpec* spec = MaterialNodeSpecFor(kind);
    return spec ? spec->name : "Expression";
}

}  // namespace Luma::Material
