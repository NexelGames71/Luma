#include "Luma/Renderer/MaterialShader.h"
#include "Luma/Renderer/Material.h"

namespace Luma {
namespace Renderer2 {

// ============================================================================
// Material Shader
// ============================================================================

MaterialShader::MaterialShader() {
}

MaterialShader::~MaterialShader() {
}

// ============================================================================
// Default Material Shader
// ============================================================================

DefaultMaterialShader::DefaultMaterialShader() {
    m_shaderName = "DefaultPBR";
}

DefaultMaterialShader::~DefaultMaterialShader() {
    if (m_parameterStruct) {
        delete m_parameterStruct;
    }
    if (m_parameterInstance) {
        delete m_parameterInstance;
    }
}

void DefaultMaterialShader::BindMaterialParameters(Material* material) {
    if (!material || !m_parameterInstance) {
        return;
    }
    
    // Update parameter instance with material data
    material->UpdateParameterStruct(m_parameterInstance);
}

bool DefaultMaterialShader::Initialize() {
    // Create parameter struct for PBR material
    m_parameterStruct = new Shader::ShaderParameterStruct("PBRMaterial");
    
    // Add common PBR parameters
    m_parameterStruct->AddParameter(Shader::ShaderParameter{"BaseColor", Shader::EShaderParameterType::Vector3, 0, 0});
    m_parameterStruct->AddParameter(Shader::ShaderParameter{"Opacity", Shader::EShaderParameterType::Scalar, 0, 0});
    m_parameterStruct->AddParameter(Shader::ShaderParameter{"Roughness", Shader::EShaderParameterType::Scalar, 0, 0});
    m_parameterStruct->AddParameter(Shader::ShaderParameter{"Metallic", Shader::EShaderParameterType::Scalar, 0, 0});
    m_parameterStruct->AddParameter(Shader::ShaderParameter{"Specular", Shader::EShaderParameterType::Scalar, 0, 0});
    m_parameterStruct->AddParameter(Shader::ShaderParameter{"EmissiveColor", Shader::EShaderParameterType::Vector3, 0, 0});
    m_parameterStruct->AddParameter(Shader::ShaderParameter{"EmissiveIntensity", Shader::EShaderParameterType::Scalar, 0, 0});
    
    // Add texture parameters
    m_parameterStruct->AddParameter(Shader::ShaderParameter{"AlbedoTexture", Shader::EShaderParameterType::Texture, 0, 0});
    m_parameterStruct->AddParameter(Shader::ShaderParameter{"NormalMap", Shader::EShaderParameterType::Texture, 0, 0});
    m_parameterStruct->AddParameter(Shader::ShaderParameter{"RoughnessTexture", Shader::EShaderParameterType::Texture, 0, 0});
    m_parameterStruct->AddParameter(Shader::ShaderParameter{"MetallicTexture", Shader::EShaderParameterType::Texture, 0, 0});
    m_parameterStruct->AddParameter(Shader::ShaderParameter{"AOTexture", Shader::EShaderParameterType::Texture, 0, 0});
    
    // Calculate struct size
    m_parameterStruct->CalculateSize();
    
    // Create parameter instance
    m_parameterInstance = new Shader::ShaderParameterInstance(m_parameterStruct);
    
    return true;
}

// ============================================================================
// Material Shader Factory
// ============================================================================

unordered_map<string, MaterialShader* (*)()> MaterialShaderFactory::s_shaderCreators;

MaterialShader* MaterialShaderFactory::CreateShader(const string& shaderType) {
    auto it = s_shaderCreators.find(shaderType);
    if (it != s_shaderCreators.end()) {
        return it->second();
    }
    
    // Default to default PBR shader
    if (shaderType == "Default" || shaderType == "PBR") {
        auto* shader = new DefaultMaterialShader();
        shader->Initialize();
        return shader;
    }
    
    return nullptr;
}

void MaterialShaderFactory::RegisterShader(const string& type, MaterialShader* (*creator)()) {
    s_shaderCreators[type] = creator;
}

vector<string> MaterialShaderFactory::GetAvailableShaderTypes() {
    vector<string> types;
    for (const auto& [type, creator] : s_shaderCreators) {
        types.push_back(type);
    }
    return types;
}

} // namespace Renderer2
} // namespace Luma