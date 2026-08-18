#include "Luma/Shader/Shader.h"
#include "Luma/Shader/ShaderCompiler.h"

#include <fstream>
#include <sstream>
#include <algorithm>

namespace Luma {
namespace Shader {

// ============================================================================
// Vertex Shader
// ============================================================================

VertexShader::VertexShader(const string& name)
    : Shader(EShaderType::Vertex, name) {
}

bool VertexShader::LoadFromFile(const string& filePath, EShaderFormat format) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        m_compileError = "Failed to open file: " + filePath;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    m_source = buffer.str();
    m_format = format;
    
    return true;
}

bool VertexShader::LoadFromSource(const string& source, EShaderFormat format) {
    m_source = source;
    m_format = format;
    return true;
}

bool VertexShader::Compile(ShaderCompiler* compiler) {
    if (!compiler) {
        m_compileError = "Compiler is null";
        return false;
    }
    
    RHI::EShaderLanguage language = (m_format == EShaderFormat::HLSL) 
        ? RHI::EShaderLanguage::HLSL 
        : RHI::EShaderLanguage::GLSL;
    
    auto result = compiler->CompileShader(
        m_source,
        RHI::EShaderStage::Vertex,
        m_entryPoint,
        language
    );
    
    if (!result.success) {
        m_compileError = result.errorMessage;
        m_valid = false;
        return false;
    }
    
    m_bytecode = std::move(result.bytecode);
    m_compiled = true;
    m_valid = true;
    return true;
}

// ============================================================================
// Fragment Shader
// ============================================================================

FragmentShader::FragmentShader(const string& name)
    : Shader(EShaderType::Fragment, name) {
}

bool FragmentShader::LoadFromFile(const string& filePath, EShaderFormat format) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        m_compileError = "Failed to open file: " + filePath;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    m_source = buffer.str();
    m_format = format;
    
    return true;
}

bool FragmentShader::LoadFromSource(const string& source, EShaderFormat format) {
    m_source = source;
    m_format = format;
    return true;
}

bool FragmentShader::Compile(ShaderCompiler* compiler) {
    if (!compiler) {
        m_compileError = "Compiler is null";
        return false;
    }
    
    RHI::EShaderLanguage language = (m_format == EShaderFormat::HLSL) 
        ? RHI::EShaderLanguage::HLSL 
        : RHI::EShaderLanguage::GLSL;
    
    auto result = compiler->CompileShader(
        m_source,
        RHI::EShaderStage::Fragment,
        m_entryPoint,
        language
    );
    
    if (!result.success) {
        m_compileError = result.errorMessage;
        m_valid = false;
        return false;
    }
    
    m_bytecode = std::move(result.bytecode);
    m_compiled = true;
    m_valid = true;
    return true;
}

// ============================================================================
// Compute Shader
// ============================================================================

ComputeShader::ComputeShader(const string& name)
    : Shader(EShaderType::Compute, name) {
}

bool ComputeShader::LoadFromFile(const string& filePath, EShaderFormat format) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        m_compileError = "Failed to open file: " + filePath;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    m_source = buffer.str();
    m_format = format;
    
    return true;
}

bool ComputeShader::LoadFromSource(const string& source, EShaderFormat format) {
    m_source = source;
    m_format = format;
    return true;
}

bool ComputeShader::Compile(ShaderCompiler* compiler) {
    if (!compiler) {
        m_compileError = "Compiler is null";
        return false;
    }
    
    RHI::EShaderLanguage language = (m_format == EShaderFormat::HLSL) 
        ? RHI::EShaderLanguage::HLSL 
        : RHI::EShaderLanguage::GLSL;
    
    auto result = compiler->CompileShader(
        m_source,
        RHI::EShaderStage::Compute,
        m_entryPoint,
        language
    );
    
    if (!result.success) {
        m_compileError = result.errorMessage;
        m_valid = false;
        return false;
    }
    
    m_bytecode = std::move(result.bytecode);
    m_compiled = true;
    m_valid = true;
    return true;
}

// ============================================================================
// Shader Program
// ============================================================================

ShaderProgram::ShaderProgram(const string& name)
    : m_name(name) {
}

ShaderProgram::~ShaderProgram() {
    // Shaders are managed by the shader library, not owned by program
}

void ShaderProgram::SetVertexShader(VertexShader* shader) {
    m_vertexShader = shader;
}

void ShaderProgram::SetFragmentShader(FragmentShader* shader) {
    m_fragmentShader = shader;
}

void ShaderProgram::SetGeometryShader(Shader* shader) {
    m_geometryShader = shader;
}

void ShaderProgram::SetTessControlShader(Shader* shader) {
    m_tessControlShader = shader;
}

void ShaderProgram::SetTessEvalShader(Shader* shader) {
    m_tessEvalShader = shader;
}

bool ShaderProgram::Link() {
    // Validate that at least vertex and fragment shaders are set
    if (!m_vertexShader || !m_fragmentShader) {
        m_linkError = "Program requires at least vertex and fragment shaders";
        m_valid = false;
        return false;
    }
    
    // Validate that all shaders are compiled
    if (!m_vertexShader->IsCompiled() || !m_fragmentShader->IsCompiled()) {
        m_linkError = "All shaders must be compiled before linking";
        m_valid = false;
        return false;
    }
    
    if (m_geometryShader && !m_geometryShader->IsCompiled()) {
        m_linkError = "Geometry shader must be compiled before linking";
        m_valid = false;
        return false;
    }
    
    if (m_tessControlShader && !m_tessControlShader->IsCompiled()) {
        m_linkError = "Tessellation control shader must be compiled before linking";
        m_valid = false;
        return false;
    }
    
    if (m_tessEvalShader && !m_tessEvalShader->IsCompiled()) {
        m_linkError = "Tessellation evaluation shader must be compiled before linking";
        m_valid = false;
        return false;
    }
    
    // TODO: Perform actual link validation (check interface compatibility, etc.)
    // For now, just mark as linked
    m_linked = true;
    m_valid = true;
    return true;
}

// ============================================================================
// Shader Library
// ============================================================================

ShaderLibrary& ShaderLibrary::GetInstance() {
    static ShaderLibrary instance;
    return instance;
}

ShaderLibrary::~ShaderLibrary() {
    Clear();
}

void ShaderLibrary::RegisterShader(Shader* shader) {
    if (shader) {
        m_shaders.push_back(shader);
    }
}

void ShaderLibrary::UnregisterShader(Shader* shader) {
    auto it = std::find(m_shaders.begin(), m_shaders.end(), shader);
    if (it != m_shaders.end()) {
        m_shaders.erase(it);
    }
}

Shader* ShaderLibrary::GetShader(const string& name) {
    for (auto* shader : m_shaders) {
        if (shader->GetName() == name) {
            return shader;
        }
    }
    return nullptr;
}

void ShaderLibrary::Clear() {
    m_shaders.clear();
}

} // namespace Shader
} // namespace Luma