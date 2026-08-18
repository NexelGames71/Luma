#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/RHI/RHIShader.h"

// Main shader system for Luma Engine. Inspired by UE5's shader system but
// adapted for Luma's simpler architecture. Provides high-level shader management
// with dual language support (GLSL and HLSL).

namespace Luma {
namespace Shader {

using std::string;
using std::vector;
using std::unique_ptr;

// Forward declarations
class ShaderParameterStruct;
class ShaderCompiler;

// ============================================================================
// Shader Metadata
// ============================================================================

// Shader type
enum class EShaderType : u32 {
    Vertex,
    Fragment,
    Geometry,
    TessControl,
    TessEval,
    Compute,
};

// Shader format
enum class EShaderFormat : u32 {
    Unknown,
    GLSL,
    HLSL,
    SPIRV,
};

// Shader permutation
struct ShaderPermutation {
    string name;
    string value;
};

// ============================================================================
// Shader Base Class
// ============================================================================

// Base shader class for all shader types
class Shader {
public:
    virtual ~Shader() = default;
    
    // Get shader name
    const string& GetName() const { return m_name; }
    
    // Get shader type
    EShaderType GetType() const { return m_type; }
    
    // Get shader format
    EShaderFormat GetFormat() const { return m_format; }
    
    // Get entry point
    const string& GetEntryPoint() const { return m_entryPoint; }
    
    // Get shader source
    const string& GetSource() const { return m_source; }
    
    // Get compiled bytecode
    const RHI::ShaderBytecode& GetBytecode() const { return m_bytecode; }
    
    // Get shader permutation
    const vector<ShaderPermutation>& GetPermutations() const { return m_permutations; }
    
    // Get RHI shader
    RHI::RHIShader* GetRHIShader() const { return m_rhiShader; }
    
    // Set RHI shader
    void SetRHIShader(RHI::RHIShader* shader) { m_rhiShader = shader; }
    
    // Check if shader is compiled
    bool IsCompiled() const { return m_compiled; }
    
    // Check if shader is valid
    bool IsValid() const { return m_valid; }
    
    // Get compile error message
    const string& GetCompileError() const { return m_compileError; }
    
protected:
    Shader(EShaderType type, const string& name)
        : m_type(type), m_name(name) {}
    
    string m_name;
    EShaderType m_type;
    EShaderFormat m_format = EShaderFormat::Unknown;
    string m_entryPoint = "main";
    string m_source;
    RHI::ShaderBytecode m_bytecode;
    vector<ShaderPermutation> m_permutations;
    RHI::RHIShader* m_rhiShader = nullptr;
    bool m_compiled = false;
    bool m_valid = false;
    string m_compileError;
};

// ============================================================================
// Vertex Shader
// ============================================================================

class VertexShader : public Shader {
public:
    VertexShader(const string& name);
    ~VertexShader() override = default;
    
    // Load from file
    bool LoadFromFile(const string& filePath, EShaderFormat format = EShaderFormat::GLSL);
    
    // Load from source
    bool LoadFromSource(const string& source, EShaderFormat format = EShaderFormat::GLSL);
    
    // Compile shader
    bool Compile(ShaderCompiler* compiler);
};

// ============================================================================
// Fragment Shader
// ============================================================================

class FragmentShader : public Shader {
public:
    FragmentShader(const string& name);
    ~FragmentShader() override = default;
    
    // Load from file
    bool LoadFromFile(const string& filePath, EShaderFormat format = EShaderFormat::GLSL);
    
    // Load from source
    bool LoadFromSource(const string& source, EShaderFormat format = EShaderFormat::GLSL);
    
    // Compile shader
    bool Compile(ShaderCompiler* compiler);
};

// ============================================================================
// Compute Shader
// ============================================================================

class ComputeShader : public Shader {
public:
    ComputeShader(const string& name);
    ~ComputeShader() override = default;
    
    // Load from file
    bool LoadFromFile(const string& filePath, EShaderFormat format = EShaderFormat::GLSL);
    
    // Load from source
    bool LoadFromSource(const string& source, EShaderFormat format = EShaderFormat::GLSL);
    
    // Compile shader
    bool Compile(ShaderCompiler* compiler);
};

// ============================================================================
// Shader Program
// ============================================================================

// Complete shader program with multiple stages
class ShaderProgram {
public:
    ShaderProgram(const string& name);
    ~ShaderProgram();
    
    // Get program name
    const string& GetName() const { return m_name; }
    
    // Set vertex shader
    void SetVertexShader(VertexShader* shader);
    
    // Set fragment shader
    void SetFragmentShader(FragmentShader* shader);
    
    // Set geometry shader
    void SetGeometryShader(Shader* shader);
    
    // Set tessellation control shader
    void SetTessControlShader(Shader* shader);
    
    // Set tessellation evaluation shader
    void SetTessEvalShader(Shader* shader);
    
    // Get vertex shader
    VertexShader* GetVertexShader() const { return m_vertexShader; }
    
    // Get fragment shader
    FragmentShader* GetFragmentShader() const { return m_fragmentShader; }
    
    // Get geometry shader
    Shader* GetGeometryShader() const { return m_geometryShader; }
    
    // Get tessellation control shader
    Shader* GetTessControlShader() const { return m_tessControlShader; }
    
    // Get tessellation evaluation shader
    Shader* GetTessEvalShader() const { return m_tessEvalShader; }
    
    // Check if program is linked
    bool IsLinked() const { return m_linked; }
    
    // Check if program is valid
    bool IsValid() const { return m_valid; }
    
    // Link program
    bool Link();
    
    // Get link error message
    const string& GetLinkError() const { return m_linkError; }
    
private:
    string m_name;
    VertexShader* m_vertexShader = nullptr;
    FragmentShader* m_fragmentShader = nullptr;
    Shader* m_geometryShader = nullptr;
    Shader* m_tessControlShader = nullptr;
    Shader* m_tessEvalShader = nullptr;
    bool m_linked = false;
    bool m_valid = false;
    string m_linkError;
};

// ============================================================================
// Shader Library
// ============================================================================

// Global shader library for managing all shaders
class ShaderLibrary {
public:
    static ShaderLibrary& GetInstance();
    
    // Register shader
    void RegisterShader(Shader* shader);
    
    // Unregister shader
    void UnregisterShader(Shader* shader);
    
    // Get shader by name
    Shader* GetShader(const string& name);
    
    // Get all shaders
    const vector<Shader*>& GetAllShaders() const { return m_shaders; }
    
    // Clear all shaders
    void Clear();
    
private:
    ShaderLibrary() = default;
    ~ShaderLibrary();
    
    vector<Shader*> m_shaders;
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Create vertex shader from file
inline VertexShader* CreateVertexShader(const string& name, const string& filePath, EShaderFormat format = EShaderFormat::GLSL) {
    auto shader = new VertexShader(name);
    shader->LoadFromFile(filePath, format);
    return shader;
}

// Create fragment shader from file
inline FragmentShader* CreateFragmentShader(const string& name, const string& filePath, EShaderFormat format = EShaderFormat::GLSL) {
    auto shader = new FragmentShader(name);
    shader->LoadFromFile(filePath, format);
    return shader;
}

// Create compute shader from file
inline ComputeShader* CreateComputeShader(const string& name, const string& filePath, EShaderFormat format = EShaderFormat::GLSL) {
    auto shader = new ComputeShader(name);
    shader->LoadFromFile(filePath, format);
    return shader;
}

} // namespace Shader
} // namespace Luma