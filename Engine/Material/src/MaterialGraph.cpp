#include "Luma/Material/MaterialGraph.h"

#include <functional>
#include <unordered_set>

namespace Luma::Material {

const std::vector<MaterialInputInfo>& MaterialInputs() {
    static const std::vector<MaterialInputInfo> kInputs = {
        {"Base Color", MaterialProperty::BaseColor,
         "Diffuse color of the surface"},
        {"Metallic", MaterialProperty::Metallic,
         "0 = dielectric, 1 = metal"},
        {"Specular", MaterialProperty::Specular,
         "Specular intensity for non-metals"},
        {"Roughness", MaterialProperty::Roughness,
         "Surface roughness (0 = smooth, 1 = rough)"},
        {"Normal", MaterialProperty::Normal,
         "Surface normal (tangent or world space)"},
        {"Tangent", MaterialProperty::Tangent,
         "Surface tangent"},
        {"Emissive Color", MaterialProperty::EmissiveColor,
         "Self-emitted light color"},
        {"Opacity", MaterialProperty::Opacity,
         "Opacity (translucent)"},
        {"Opacity Mask", MaterialProperty::OpacityMask,
         "Masked opacity clip value"},
        {"World Position Offset", MaterialProperty::WorldPositionOffset,
         "Per-vertex world-space displacement"},
        {"Subsurface Color", MaterialProperty::SubsurfaceColor,
         "Subsurface scattering color"},
        {"Ambient Occlusion", MaterialProperty::AmbientOcclusion,
         "AO multiplier into the GBuffer"},
        {"Refraction", MaterialProperty::Refraction,
         "Refraction index / offset"},
        {"Pixel Depth Offset", MaterialProperty::PixelDepthOffset,
         "Per-pixel depth offset"},
        // --- Material Output > Volume / Thickness (Blender-style) ---------
        {"Volume Color", MaterialProperty::VolumeColor,
         "Volume scatter/emission color"},
        {"Volume Density", MaterialProperty::VolumeDensity,
         "Volume density"},
        {"Volume Flame", MaterialProperty::VolumeFlame,
         "Fire density inside the volume"},
        {"Volume Temperature", MaterialProperty::VolumeTemperature,
         "0..1 maps to 0..1000 kelvin"},
        {"Thickness", MaterialProperty::Thickness,
         "EEVEE SSS thickness approximation"},
    };
    return kInputs;
}

MaterialValueType MaterialPropertyValueType(MaterialProperty p) {
    switch (p) {
        case MaterialProperty::BaseColor:
        case MaterialProperty::Normal:
        case MaterialProperty::Tangent:
        case MaterialProperty::EmissiveColor:
        case MaterialProperty::WorldPositionOffset:
        case MaterialProperty::SubsurfaceColor:
        case MaterialProperty::VolumeColor:
            return MCT_Float3;
        default:
            return MCT_Float1;
    }
}

ExpressionId MaterialGraph::AddNode(ExpressionKind kind) {
    MaterialExpression node = MakeExpression(kind, m_nextId++);
    m_nodes.push_back(std::move(node));
    return m_nodes.back().id;
}

void MaterialGraph::RemoveNode(ExpressionId id) {
    for (auto& node : m_nodes) {
        for (auto& in : node.inputs) {
            if (in.expression == id) in.Disconnect();
        }
    }
    m_nodes.erase(std::remove_if(m_nodes.begin(), m_nodes.end(),
                                [id](const MaterialExpression& n) {
                                    return n.id == id;
                                }),
                  m_nodes.end());
}

void MaterialGraph::Clear() {
    m_nodes.clear();
    m_nextId = 1;
}

MaterialExpression* MaterialGraph::Find(ExpressionId id) {
    for (auto& node : m_nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

const MaterialExpression* MaterialGraph::Find(ExpressionId id) const {
    for (const auto& node : m_nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

bool MaterialGraph::Contains(ExpressionId id) const {
    return Find(id) != nullptr;
}

void MaterialGraph::ForEachReachable(
    ExpressionId from,
    const std::function<void(MaterialExpression&)>& visit) {
    std::unordered_set<ExpressionId> visited;
    // Iterative DFS so deep graphs can't overflow the stack.
    std::vector<ExpressionId> stack{from};
    while (!stack.empty()) {
        ExpressionId id = stack.back();
        stack.pop_back();
        if (id == kInvalidExpressionId || !visited.insert(id).second) continue;
        MaterialExpression* node = Find(id);
        if (!node) continue;
        visit(*node);
        for (const auto& in : node->inputs) {
            stack.push_back(in.expression);
        }
    }
}

}  // namespace Luma::Material
