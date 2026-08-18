#include "Luma/Renderer/Material.h"

namespace Luma {
namespace Renderer2 {

// ============================================================================
// Material
// ============================================================================

Material::Material()
    : m_blendMode(EMaterialBlendMode::Opaque)
    , m_shadingModel(EMaterialShadingModel::DefaultLit)
    , m_opacity(1.0f)
    , m_opacityMask(0.5f)
    , m_roughness(0.5f)
    , m_metallic(0.0f)
    , m_specular(0.5f)
    , m_baseColor(Vec3(1.0f, 1.0f, 1.0f))
    , m_emissiveColor(Vec3(0.0f, 0.0f, 0.0f))
    , m_emissiveIntensity(0.0f)
    , m_normalMap(0)
    , m_albedoTexture(0)
    , m_roughnessTexture(0)
    , m_metallicTexture(0)
    , m_aoTexture(0)
    , m_materialShader(nullptr) {
}

Material::~Material() {
}

void Material::AddParameter(const MaterialParameter& parameter) {
    m_parameters.push_back(parameter);
}

const MaterialParameter* Material::GetParameter(const string& name) const {
    for (const auto& param : m_parameters) {
        if (param.name == name) {
            return &param;
        }
    }
    return nullptr;
}

void Material::UpdateParameterStruct(Shader::ShaderParameterInstance* instance) {
    if (!instance) {
        return;
    }
    
    // Update base material parameters
    instance->SetVector3("BaseColor", m_baseColor);
    instance->SetScalar("Opacity", m_opacity);
    instance->SetScalar("Roughness", m_roughness);
    instance->SetScalar("Metallic", m_metallic);
    instance->SetScalar("Specular", m_specular);
    instance->SetVector3("EmissiveColor", m_emissiveColor);
    instance->SetScalar("EmissiveIntensity", m_emissiveIntensity);
    
    // Update texture parameters
    if (m_albedoTexture != 0) {
        instance->SetTexture("AlbedoTexture", m_albedoTexture);
    }
    if (m_normalMap != 0) {
        instance->SetTexture("NormalMap", m_normalMap);
    }
    if (m_roughnessTexture != 0) {
        instance->SetTexture("RoughnessTexture", m_roughnessTexture);
    }
    if (m_metallicTexture != 0) {
        instance->SetTexture("MetallicTexture", m_metallicTexture);
    }
    if (m_aoTexture != 0) {
        instance->SetTexture("AOTexture", m_aoTexture);
    }
    
    // Update custom parameters
    for (const auto& param : m_parameters) {
        switch (param.type) {
            case EMaterialParameterType::Scalar:
                instance->SetScalar(param.name, param.scalar);
                break;
            case EMaterialParameterType::Vector2:
                instance->SetVector2(param.name, param.vector2);
                break;
            case EMaterialParameterType::Vector3:
                instance->SetVector3(param.name, param.vector3);
                break;
            case EMaterialParameterType::Vector4:
            case EMaterialParameterType::Color:
                instance->SetVector4(param.name, param.vector4);
                break;
            case EMaterialParameterType::Texture:
            case EMaterialParameterType::NormalMap:
                instance->SetTexture(param.name, param.textureHandle);
                break;
            default:
                break;
        }
    }
    
    instance->UpdateBuffer();
}

// ============================================================================
// Material Registry
// ============================================================================

MaterialRegistry& MaterialRegistry::GetInstance() {
    static MaterialRegistry instance;
    return instance;
}

MaterialRegistry::~MaterialRegistry() {
    Clear();
}

void MaterialRegistry::RegisterMaterial(Material* material) {
    if (material) {
        m_materials.push_back(material);
        m_materialMap[material->GetName()] = material;
    }
}

void MaterialRegistry::UnregisterMaterial(const string& name) {
    auto it = m_materialMap.find(name);
    if (it != m_materialMap.end()) {
        auto materialIt = std::find(m_materials.begin(), m_materials.end(), it->second);
        if (materialIt != m_materials.end()) {
            m_materials.erase(materialIt);
        }
        m_materialMap.erase(it);
    }
}

Material* MaterialRegistry::GetMaterial(const string& name) const {
    auto it = m_materialMap.find(name);
    if (it != m_materialMap.end()) {
        return it->second;
    }
    return nullptr;
}

void MaterialRegistry::Clear() {
    m_materials.clear();
    m_materialMap.clear();
}

} // namespace Renderer2
} // namespace Luma