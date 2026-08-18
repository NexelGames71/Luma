#include "Luma/Material/MaterialGraphSchema.h"

#include <string>
#include <unordered_set>
#include <vector>

#include "Luma/Material/MaterialNodeSpec.h"

namespace Luma::Material {

bool MaterialGraphSchema::ArePinsCompatible(MaterialValueType inputType,
                                            MaterialValueType outputType) {
    return CanConnectMaterialValueTypes(inputType, outputType);
}

bool MaterialGraphSchema::CanCreateConnection(const MaterialGraph& graph,
                                              const MaterialExpression& fromNode,
                                              i32 outputIndex,
                                              const MaterialExpression& toNode,
                                              i32 inputIndex) {
    if (fromNode.id == toNode.id) return false;
    const ExpressionInput* in = toNode.Input(inputIndex);
    if (!in) return false;
    const ExpressionOutput* out = (outputIndex >= 0 &&
                                   static_cast<usize>(outputIndex) <
                                       fromNode.outputs.size())
                                      ? &fromNode.outputs[static_cast<usize>(
                                            outputIndex)]
                                      : nullptr;
    if (!out) return false;
    if (!ArePinsCompatible(in->expected, out->type)) return false;

    // Loop detection (UE ConnectionCausesLoop): would connecting from -> to
    // close a cycle? Follow `fromNode`'s transitive inputs; if `toNode` is
    // reachable the connection would create a loop.
    std::unordered_set<ExpressionId> visited;
    std::vector<const MaterialExpression*> stack;
    const MaterialExpression* src = graph.Find(fromNode.id);
    if (!src) return true;  // source vanished; allow (compiler will flag it)
    for (const auto& input : src->inputs) {
        if (!input.IsConnected()) continue;
        const MaterialExpression* n = graph.Find(input.expression);
        if (n) stack.push_back(n);
    }
    while (!stack.empty()) {
        const MaterialExpression* n = stack.back();
        stack.pop_back();
        if (n->id == toNode.id) return false;  // cycle
        if (!visited.insert(n->id).second) continue;
        for (const auto& input : n->inputs) {
            if (!input.IsConnected()) continue;
            const MaterialExpression* next = graph.Find(input.expression);
            if (next) stack.push_back(next);
        }
    }
    return true;
}

MaterialValueType MaterialGraphSchema::GetMaterialIOValueType(
    MaterialProperty p) {
    return MaterialPropertyValueType(p);
}

Math::Vec3 MaterialGraphSchema::GetPinTypeColor(MaterialValueType type) {
    switch (type) {
        case MCT_Float1: return {172.0f / 255.0f, 172.0f / 255.0f, 176.0f / 255.0f};
        case MCT_Float2: return {74.0f / 255.0f, 200.0f / 255.0f, 190.0f / 255.0f};
        case MCT_Float3: return {120.0f / 255.0f, 198.0f / 255.0f, 96.0f / 255.0f};
        case MCT_Float4: return {232.0f / 255.0f, 232.0f / 255.0f, 238.0f / 255.0f};
        case MCT_Texture2D: return {96.0f / 255.0f, 146.0f / 255.0f, 224.0f / 255.0f};
        case MCT_StaticBool: return {232.0f / 255.0f, 158.0f / 255.0f, 64.0f / 255.0f};
        case MCT_Int: return {96.0f / 255.0f, 130.0f / 255.0f, 182.0f / 255.0f};
        case MCT_Menu: return {176.0f / 255.0f, 120.0f / 255.0f, 204.0f / 255.0f};
        case MCT_Unknown: return {140.0f / 255.0f, 140.0f / 255.0f, 150.0f / 255.0f};
        default: return {160.0f / 255.0f, 160.0f / 255.0f, 160.0f / 255.0f};
    }
}

const MaterialGraphSchema::PaletteCategory* MaterialGraphSchema::Palette(
    int& outCount) {
    // The palette is derived from the data-driven node registry
    // (MaterialNodeSpecs): every registered node appears under its declared
    // category, in first-declared category order. Adding a node to the
    // registry makes it spawnable here automatically.
    struct Lazy {
        std::vector<PaletteCategory> cats;
        std::vector<std::vector<PaletteItem>> items;
        Lazy() {
            const auto& specs = MaterialNodeSpecs();
            std::vector<std::string> order;  // first-seen category order
            for (const auto& spec : specs) {
                bool seen = false;
                for (const auto& cat : order) {
                    if (cat == spec.category) {
                        seen = true;
                        break;
                    }
                }
                if (!seen) order.emplace_back(spec.category);
            }
            items.resize(order.size());
            for (const auto& spec : specs) {
                for (usize c = 0; c < order.size(); ++c) {
                    if (order[c] != spec.category) continue;
                    items[c].push_back(
                        PaletteItem{spec.name.data(), spec.kind});
                    break;
                }
            }
            for (usize c = 0; c < order.size(); ++c) {
                cats.push_back(PaletteCategory{
                    order[c].c_str(), items[c].data(),
                    static_cast<int>(items[c].size())});
            }
        }
    };
    static const Lazy kLazy;
    outCount = static_cast<int>(kLazy.cats.size());
    return kLazy.cats.data();
}

PinCategory MaterialGraphSchema::GetInputPinCategory(
    const MaterialExpression& node, i32 inputIndex) {
    (void)node;
    (void)inputIndex;
    // Luma has no required/optional distinction yet: every expression input
    // is optional. The Material Output node's pins are MaterialInput (set by
    // the editor directly).
    return PinCategory::Optional;
}

}  // namespace Luma::Material
