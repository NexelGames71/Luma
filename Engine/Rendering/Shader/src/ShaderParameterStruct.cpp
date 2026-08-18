#include "Luma/Shader/ShaderParameterStruct.h"

#include <cstring>
#include <algorithm>

namespace Luma {
namespace Shader {

// ============================================================================
// Parameter Struct
// ============================================================================

ShaderParameterStruct::ShaderParameterStruct(const string& name)
    : m_name(name) {
}

ShaderParameterStruct::~ShaderParameterStruct() {
}

void ShaderParameterStruct::AddParameter(const ShaderParameter& param) {
    m_parameters.push_back(param);
    m_parameterMap[param.name] = static_cast<u32>(m_parameters.size() - 1);
}

const ShaderParameter* ShaderParameterStruct::GetParameter(const string& name) const {
    auto it = m_parameterMap.find(name);
    if (it != m_parameterMap.end()) {
        return &m_parameters[it->second];
    }
    return nullptr;
}

void ShaderParameterStruct::CalculateSize() {
    u32 size = 0;
    for (auto& param : m_parameters) {
        param.offset = size;
        
        switch (param.type) {
            case EShaderParameterType::Scalar:
                size += sizeof(f32);
                break;
            case EShaderParameterType::Vector2:
                size += sizeof(Vec2);
                break;
            case EShaderParameterType::Vector3:
                size += sizeof(Vec3);
                break;
            case EShaderParameterType::Vector4:
                size += sizeof(Vec4);
                break;
            case EShaderParameterType::Matrix4x4:
                size += sizeof(Mat4);
                break;
            case EShaderParameterType::Texture:
            case EShaderParameterType::Sampler:
                size += sizeof(u64);
                break;
            case EShaderParameterType::Struct:
            case EShaderParameterType::Array:
                size += param.size;
                break;
        }
        
        param.size = size - param.offset;
    }
    
    m_size = size;
}

bool ShaderParameterStruct::HasParameter(const string& name) const {
    return m_parameterMap.find(name) != m_parameterMap.end();
}

// ============================================================================
// Parameter Instance
// ============================================================================

ShaderParameterInstance::ShaderParameterInstance(const ShaderParameterStruct* structDef)
    : m_structDef(structDef) {
    if (structDef) {
        m_dataSize = structDef->GetSize();
        m_data = new u8[m_dataSize];
        std::memset(m_data, 0, m_dataSize);
    }
}

ShaderParameterInstance::~ShaderParameterInstance() {
    if (m_data) {
        delete[] m_data;
    }
}

bool ShaderParameterInstance::SetScalar(const string& name, f32 value) {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Scalar) {
        return false;
    }
    
    std::memcpy(m_data + param->offset, &value, sizeof(f32));
    m_parameterData[name].type = EShaderParameterType::Scalar;
    m_parameterData[name].scalar = value;
    return true;
}

bool ShaderParameterInstance::SetVector2(const string& name, const Vec2& value) {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Vector2) {
        return false;
    }
    
    std::memcpy(m_data + param->offset, &value, sizeof(Vec2));
    m_parameterData[name].type = EShaderParameterType::Vector2;
    m_parameterData[name].vector2 = value;
    return true;
}

bool ShaderParameterInstance::SetVector3(const string& name, const Vec3& value) {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Vector3) {
        return false;
    }
    
    std::memcpy(m_data + param->offset, &value, sizeof(Vec3));
    m_parameterData[name].type = EShaderParameterType::Vector3;
    m_parameterData[name].vector3 = value;
    return true;
}

bool ShaderParameterInstance::SetVector4(const string& name, const Vec4& value) {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Vector4) {
        return false;
    }
    
    std::memcpy(m_data + param->offset, &value, sizeof(Vec4));
    m_parameterData[name].type = EShaderParameterType::Vector4;
    m_parameterData[name].vector4 = value;
    return true;
}

bool ShaderParameterInstance::SetMatrix4x4(const string& name, const Mat4& value) {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Matrix4x4) {
        return false;
    }
    
    std::memcpy(m_data + param->offset, &value, sizeof(Mat4));
    m_parameterData[name].type = EShaderParameterType::Matrix4x4;
    m_parameterData[name].matrix4x4 = value;
    return true;
}

bool ShaderParameterInstance::SetTexture(const string& name, u64 textureHandle) {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Texture) {
        return false;
    }
    
    std::memcpy(m_data + param->offset, &textureHandle, sizeof(u64));
    m_parameterData[name].type = EShaderParameterType::Texture;
    m_parameterData[name].textureHandle = textureHandle;
    return true;
}

bool ShaderParameterInstance::SetSampler(const string& name, u64 samplerHandle) {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Sampler) {
        return false;
    }
    
    std::memcpy(m_data + param->offset, &samplerHandle, sizeof(u64));
    m_parameterData[name].type = EShaderParameterType::Sampler;
    m_parameterData[name].samplerHandle = samplerHandle;
    return true;
}

bool ShaderParameterInstance::GetScalar(const string& name, f32& value) const {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Scalar) {
        return false;
    }
    
    std::memcpy(&value, m_data + param->offset, sizeof(f32));
    return true;
}

bool ShaderParameterInstance::GetVector2(const string& name, Vec2& value) const {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Vector2) {
        return false;
    }
    
    std::memcpy(&value, m_data + param->offset, sizeof(Vec2));
    return true;
}

bool ShaderParameterInstance::GetVector3(const string& name, Vec3& value) const {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Vector3) {
        return false;
    }
    
    std::memcpy(&value, m_data + param->offset, sizeof(Vec3));
    return true;
}

bool ShaderParameterInstance::GetVector4(const string& name, Vec4& value) const {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Vector4) {
        return false;
    }
    
    std::memcpy(&value, m_data + param->offset, sizeof(Vec4));
    return true;
}

bool ShaderParameterInstance::GetMatrix4x4(const string& name, Mat4& value) const {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Matrix4x4) {
        return false;
    }
    
    std::memcpy(&value, m_data + param->offset, sizeof(Mat4));
    return true;
}

bool ShaderParameterInstance::GetTexture(const string& name, u64& textureHandle) const {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Texture) {
        return false;
    }
    
    std::memcpy(&textureHandle, m_data + param->offset, sizeof(u64));
    return true;
}

bool ShaderParameterInstance::GetSampler(const string& name, u64& samplerHandle) const {
    const auto* param = m_structDef->GetParameter(name);
    if (!param || param->type != EShaderParameterType::Sampler) {
        return false;
    }
    
    std::memcpy(&samplerHandle, m_data + param->offset, sizeof(u64));
    return true;
}

void ShaderParameterInstance::Clear() {
    if (m_data) {
        std::memset(m_data, 0, m_dataSize);
    }
    m_parameterData.clear();
}

void ShaderParameterInstance::UpdateBuffer() {
    // Update buffer with current parameter data
    for (const auto& [name, data] : m_parameterData) {
        const auto* param = m_structDef->GetParameter(name);
        if (!param) continue;
        
        switch (data.type) {
            case EShaderParameterType::Scalar:
                std::memcpy(m_data + param->offset, &data.scalar, sizeof(f32));
                break;
            case EShaderParameterType::Vector2:
                std::memcpy(m_data + param->offset, &data.vector2, sizeof(Vec2));
                break;
            case EShaderParameterType::Vector3:
                std::memcpy(m_data + param->offset, &data.vector3, sizeof(Vec3));
                break;
            case EShaderParameterType::Vector4:
                std::memcpy(m_data + param->offset, &data.vector4, sizeof(Vec4));
                break;
            case EShaderParameterType::Matrix4x4:
                std::memcpy(m_data + param->offset, &data.matrix4x4, sizeof(Mat4));
                break;
            case EShaderParameterType::Texture:
                std::memcpy(m_data + param->offset, &data.textureHandle, sizeof(u64));
                break;
            case EShaderParameterType::Sampler:
                std::memcpy(m_data + param->offset, &data.samplerHandle, sizeof(u64));
                break;
            default:
                break;
        }
    }
}

// ============================================================================
// Parameter Registry
// ============================================================================

ShaderParameterRegistry& ShaderParameterRegistry::GetInstance() {
    static ShaderParameterRegistry instance;
    return instance;
}

ShaderParameterRegistry::~ShaderParameterRegistry() {
    Clear();
}

void ShaderParameterRegistry::RegisterStruct(ShaderParameterStruct* structDef) {
    if (structDef) {
        m_structs.push_back(structDef);
        m_structMap[structDef->GetName()] = structDef;
    }
}

void ShaderParameterRegistry::UnregisterStruct(ShaderParameterStruct* structDef) {
    auto it = std::find(m_structs.begin(), m_structs.end(), structDef);
    if (it != m_structs.end()) {
        m_structs.erase(it);
    }
    m_structMap.erase(structDef->GetName());
}

const ShaderParameterStruct* ShaderParameterRegistry::GetStruct(const string& name) const {
    auto it = m_structMap.find(name);
    if (it != m_structMap.end()) {
        return it->second;
    }
    return nullptr;
}

void ShaderParameterRegistry::Clear() {
    m_structs.clear();
    m_structMap.clear();
}

} // namespace Shader
} // namespace Luma