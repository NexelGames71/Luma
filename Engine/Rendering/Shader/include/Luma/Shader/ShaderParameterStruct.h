#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"

// Shader parameter structure system. Inspired by UE5's shader parameter structs
// but adapted for Luma's simpler architecture. Provides a way to define and
// manage shader parameters in a structured way.

namespace Luma {
namespace Shader {

using std::string;
using std::vector;
using std::unordered_map;
using namespace Luma::Math;

// ============================================================================
// Parameter Types
// ============================================================================

// Shader parameter type
enum class EShaderParameterType : u32 {
    Scalar,         // f32
    Vector2,        // Vec2
    Vector3,        // Vec3
    Vector4,        // Vec4
    Matrix4x4,      // Mat4
    Texture,        // Texture handle
    Sampler,        // Sampler state
    Struct,         // Custom struct
    Array,          // Array of values
};

// Parameter data
struct ShaderParameterData {
    EShaderParameterType type;
    union {
        f32 scalar;
        Vec2 vector2;
        Vec3 vector3;
        Vec4 vector4;
        Mat4 matrix4x4;
        u64 textureHandle;
        u64 samplerHandle;
    };
    vector<f32> arrayData;
    
    ShaderParameterData() : type(EShaderParameterType::Scalar), scalar(0.0f) {
        vector2 = Vec2(0.0f, 0.0f);
        vector3 = Vec3(0.0f, 0.0f, 0.0f);
        vector4 = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        matrix4x4 = Mat4::Identity();
        textureHandle = 0;
        samplerHandle = 0;
    }
};

// ============================================================================
// Parameter Metadata
// ============================================================================

// Shader parameter metadata
struct ShaderParameter {
    string name;
    EShaderParameterType type;
    u32 offset = 0;
    u32 size = 0;
    u32 arraySize = 1;
    u32 binding = 0;
    u32 set = 0;
    bool isReadOnly = false;
    bool isOptional = false;
};

// ============================================================================
// Parameter Struct
// ============================================================================

// Shader parameter struct definition
class ShaderParameterStruct {
public:
    ShaderParameterStruct(const string& name);
    ~ShaderParameterStruct();
    
    // Get struct name
    const string& GetName() const { return m_name; }
    
    // Get struct size
    u32 GetSize() const { return m_size; }
    
    // Add parameter
    void AddParameter(const ShaderParameter& param);
    
    // Get parameter by name
    const ShaderParameter* GetParameter(const string& name) const;
    
    // Get all parameters
    const vector<ShaderParameter>& GetParameters() const { return m_parameters; }
    
    // Calculate struct size
    void CalculateSize();
    
    // Check if parameter exists
    bool HasParameter(const string& name) const;
    
private:
    string m_name;
    vector<ShaderParameter> m_parameters;
    unordered_map<string, u32> m_parameterMap;
    u32 m_size = 0;
};

// ============================================================================
// Parameter Instance
// ============================================================================

// Instance of a parameter struct with actual data
class ShaderParameterInstance {
public:
    ShaderParameterInstance(const ShaderParameterStruct* structDef);
    ~ShaderParameterInstance();
    
    // Get struct definition
    const ShaderParameterStruct* GetStructDef() const { return m_structDef; }
    
    // Get data buffer
    const void* GetData() const { return m_data; }
    
    // Get data size
    u32 GetDataSize() const { return m_dataSize; }
    
    // Set scalar parameter
    bool SetScalar(const string& name, f32 value);
    
    // Set vector2 parameter
    bool SetVector2(const string& name, const Vec2& value);
    
    // Set vector3 parameter
    bool SetVector3(const string& name, const Vec3& value);
    
    // Set vector4 parameter
    bool SetVector4(const string& name, const Vec4& value);
    
    // Set matrix4x4 parameter
    bool SetMatrix4x4(const string& name, const Mat4& value);
    
    // Set texture parameter
    bool SetTexture(const string& name, u64 textureHandle);
    
    // Set sampler parameter
    bool SetSampler(const string& name, u64 samplerHandle);
    
    // Get scalar parameter
    bool GetScalar(const string& name, f32& value) const;
    
    // Get vector2 parameter
    bool GetVector2(const string& name, Vec2& value) const;
    
    // Get vector3 parameter
    bool GetVector3(const string& name, Vec3& value) const;
    
    // Get vector4 parameter
    bool GetVector4(const string& name, Vec4& value) const;
    
    // Get matrix4x4 parameter
    bool GetMatrix4x4(const string& name, Mat4& value) const;
    
    // Get texture parameter
    bool GetTexture(const string& name, u64& textureHandle) const;
    
    // Get sampler parameter
    bool GetSampler(const string& name, u64& samplerHandle) const;
    
    // Clear all data
    void Clear();
    
    // Update data buffer
    void UpdateBuffer();
    
private:
    const ShaderParameterStruct* m_structDef;
    u8* m_data = nullptr;
    u32 m_dataSize = 0;
    unordered_map<string, ShaderParameterData> m_parameterData;
};

// ============================================================================
// Parameter Registry
// ============================================================================

// Global registry for parameter struct definitions
class ShaderParameterRegistry {
public:
    static ShaderParameterRegistry& GetInstance();
    
    // Register parameter struct
    void RegisterStruct(ShaderParameterStruct* structDef);
    
    // Unregister parameter struct
    void UnregisterStruct(ShaderParameterStruct* structDef);
    
    // Get parameter struct by name
    const ShaderParameterStruct* GetStruct(const string& name) const;
    
    // Get all parameter structs
    const vector<ShaderParameterStruct*>& GetAllStructs() const { return m_structs; }
    
    // Clear all parameter structs
    void Clear();
    
private:
    ShaderParameterRegistry() = default;
    ~ShaderParameterRegistry();
    
    vector<ShaderParameterStruct*> m_structs;
    unordered_map<string, ShaderParameterStruct*> m_structMap;
};

// ============================================================================
// Convenience Macros
// ============================================================================

// Define a scalar parameter
#define SHADER_PARAM_SCALAR(name, offset) \
    ShaderParameter{#name, EShaderParameterType::Scalar, offset, sizeof(f32)}

// Define a vector2 parameter
#define SHADER_PARAM_VECTOR2(name, offset) \
    ShaderParameter{#name, EShaderParameterType::Vector2, offset, sizeof(Vec2)}

// Define a vector3 parameter
#define SHADER_PARAM_VECTOR3(name, offset) \
    ShaderParameter{#name, EShaderParameterType::Vector3, offset, sizeof(Vec3)}

// Define a vector4 parameter
#define SHADER_PARAM_VECTOR4(name, offset) \
    ShaderParameter{#name, EShaderParameterType::Vector4, offset, sizeof(Vec4)}

// Define a matrix4x4 parameter
#define SHADER_PARAM_MATRIX4X4(name, offset) \
    ShaderParameter{#name, EShaderParameterType::Matrix4x4, offset, sizeof(Mat4)}

} // namespace Shader
} // namespace Luma