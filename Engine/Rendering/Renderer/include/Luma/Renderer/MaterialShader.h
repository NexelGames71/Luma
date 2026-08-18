#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Luma/Core/Types.h"
#include "Luma/Shader/Shader.h"
#include "Luma/Shader/ShaderParameterStruct.h"

// Material shader binding. Provides the interface between materials
// and the shader system for efficient parameter binding.

namespace Luma {
namespace Renderer2 {

using std::string;
using std::vector;
using std::unordered_map;

// Forward declarations
class Material;

// ============================================================================
// Material Shader
// ============================================================================

// Material shader interface
class MaterialShader {
public:
    MaterialShader();
    virtual ~MaterialShader();
    
    // Get shader name
    const string& GetShaderName() const { return m_shaderName; }
    
    // Set shader name
    void SetShaderName(const string& name) { m_shaderName = name; }
    
    // Get vertex shader
    Shader::VertexShader* GetVertexShader() const { return m_vertexShader; }
    
    // Set vertex shader
    void SetVertexShader(Shader::VertexShader* shader) { m_vertexShader = shader; }
    
    // Get fragment shader
    Shader::FragmentShader* GetFragmentShader() const { return m_fragmentShader; }
    
    // Set fragment shader
    void SetFragmentShader(Shader::FragmentShader* shader) { m_fragmentShader = shader; }
    
    // Get shader program
    Shader::ShaderProgram* GetShaderProgram() const { return m_shaderProgram; }
    
    // Set shader program
    void SetShaderProgram(Shader::ShaderProgram* program) { m_shaderProgram = program; }
    
    // Bind material parameters to shader
    virtual void BindMaterialParameters(Material* material) = 0;
    
    // Check if shader is valid
    bool IsValid() const { return m_vertexShader != nullptr && m_fragmentShader != nullptr; }
    
protected:
    string m_shaderName;
    Shader::VertexShader* m_vertexShader = nullptr;
    Shader::FragmentShader* m_fragmentShader = nullptr;
    Shader::ShaderProgram* m_shaderProgram = nullptr;
};

// ============================================================================
// Default Material Shader
// ============================================================================

// Default PBR material shader implementation
class DefaultMaterialShader : public MaterialShader {
public:
    DefaultMaterialShader();
    ~DefaultMaterialShader() override;
    
    // Bind material parameters to shader
    void BindMaterialParameters(Material* material) override;
    
    // Initialize shader
    bool Initialize();
    
private:
    Shader::ShaderParameterStruct* m_parameterStruct = nullptr;
    Shader::ShaderParameterInstance* m_parameterInstance = nullptr;
};

// ============================================================================
// Material Shader Factory
// ============================================================================

// Factory for creating material shaders
class MaterialShaderFactory {
public:
    static MaterialShader* CreateShader(const string& shaderType);
    
    // Register custom shader type
    static void RegisterShader(const string& type, MaterialShader* (*creator)());
    
    // Get available shader types
    static vector<string> GetAvailableShaderTypes();
    
private:
    static unordered_map<string, MaterialShader* (*)()> s_shaderCreators;
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Create default material shader
inline MaterialShader* CreateDefaultMaterialShader() {
    return MaterialShaderFactory::CreateShader("Default");
}

} // namespace Renderer2
} // namespace Luma