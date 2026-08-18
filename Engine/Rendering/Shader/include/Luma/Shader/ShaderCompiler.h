#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Luma/Core/Types.h"
#include "Luma/RHI/RHIShader.h"

#include <string>
#include <vector>
#include <unordered_map>

// Shader compiler system. Supports dual language compilation (GLSL and HLSL)
// to SPIR-V for Vulkan. Inspired by UE5's shader compilation system but
// adapted for Luma's simpler architecture.

namespace Luma {
namespace Shader {

using std::string;
using std::vector;
using std::unordered_map;

// ============================================================================
// Compiler Configuration
// ============================================================================

// Compiler optimization level
enum class EShaderOptimizationLevel : u32 {
    None,       // No optimization
    O0,         // No optimization
    O1,         // Basic optimization
    O2,         // Standard optimization
    O3,         // Aggressive optimization
};

// Compiler target
enum class EShaderTarget : u32 {
    Vulkan,     // Vulkan SPIR-V
    OpenGL,     // OpenGL SPIR-V
    DirectX,    // DirectX shader bytecode
};

// Compiler options
struct ShaderCompilerOptions {
    EShaderOptimizationLevel optimizationLevel = EShaderOptimizationLevel::O2;
    bool debugInfo = false;
    bool validate = true;
    bool generateReflection = true;
    bool generateIncludeDirectory = false;
    string includeDirectory;
    vector<string> defines;
    vector<string> macroDefinitions;
    
    // Vulkan-specific options
    u32 vulkanVersion = 0;  // 0 = use default
    bool spirvVersion14 = false;
    
    // HLSL-specific options
    string hlslProfile;  // e.g., "vs_5_0", "ps_5_0"
    bool hlslEnable16bitTypes = false;
    
    // GLSL-specific options
    string glslVersion;  // e.g., "450", "460"
    bool glslEs = false;
};

// ============================================================================
// Compilation Result
// ============================================================================

// Shader compilation result
struct ShaderCompilationResult {
    bool success = false;
    string errorMessage;
    string warningMessage;
    RHI::ShaderBytecode bytecode;
    RHI::ShaderReflection reflection;
    u64 compilationTimeMs = 0;
    
    // Source mapping for debugging
    struct SourceMapping {
        u32 sourceLine;
        u32 sourceColumn;
        u32 generatedLine;
        u32 generatedColumn;
    };
    vector<SourceMapping> sourceMappings;
};

// ============================================================================
// Shader Compiler
// ============================================================================

// Base shader compiler interface
class ShaderCompiler {
public:
    virtual ~ShaderCompiler() = default;
    
    // Compile shader from source
    virtual ShaderCompilationResult CompileShader(
        const string& source,
        RHI::EShaderStage stage,
        const string& entryPoint,
        RHI::EShaderLanguage language,
        const ShaderCompilerOptions& options = ShaderCompilerOptions()) = 0;
    
    // Compile shader from file
    virtual ShaderCompilationResult CompileShaderFromFile(
        const string& filePath,
        RHI::EShaderStage stage,
        const string& entryPoint,
        RHI::EShaderLanguage language,
        const ShaderCompilerOptions& options = ShaderCompilerOptions()) = 0;
    
    // Get supported languages
    virtual vector<RHI::EShaderLanguage> GetSupportedLanguages() const = 0;
    
    // Get supported stages for a language
    virtual vector<RHI::EShaderStage> GetSupportedStages(RHI::EShaderLanguage language) const = 0;
    
    // Get default options
    virtual ShaderCompilerOptions GetDefaultOptions() const { return ShaderCompilerOptions(); }
    
    // Validate shader source without compiling
    virtual bool ValidateSource(
        const string& source,
        RHI::EShaderLanguage language,
        string& errorMessage) = 0;
    
    // Preprocess shader source
    virtual string PreprocessSource(
        const string& source,
        const ShaderCompilerOptions& options,
        string& errorMessage) = 0;
};

// ============================================================================
// GLSL Compiler
// ============================================================================

// GLSL to SPIR-V compiler using glslc/glslang
class GLSLCompiler : public ShaderCompiler {
public:
    GLSLCompiler();
    ~GLSLCompiler() override;
    
    ShaderCompilationResult CompileShader(
        const string& source,
        RHI::EShaderStage stage,
        const string& entryPoint,
        RHI::EShaderLanguage language,
        const ShaderCompilerOptions& options = ShaderCompilerOptions()) override;
    
    ShaderCompilationResult CompileShaderFromFile(
        const string& filePath,
        RHI::EShaderStage stage,
        const string& entryPoint,
        RHI::EShaderLanguage language,
        const ShaderCompilerOptions& options = ShaderCompilerOptions()) override;
    
    vector<RHI::EShaderLanguage> GetSupportedLanguages() const override;
    vector<RHI::EShaderStage> GetSupportedStages(RHI::EShaderLanguage language) const override;
    
    bool ValidateSource(
        const string& source,
        RHI::EShaderLanguage language,
        string& errorMessage) override;
    
    string PreprocessSource(
        const string& source,
        const ShaderCompilerOptions& options,
        string& errorMessage) override;
    
    // Set glslc executable path
    void SetGlslcPath(const string& path) { m_glslcPath = path; }
    
    // Get glslc executable path
    const string& GetGlslcPath() const { return m_glslcPath; }
    
private:
    string m_glslcPath;
    bool m_initialized;
    
    // Run glslc compiler
    ShaderCompilationResult RunGlslc(
        const string& source,
        RHI::EShaderStage stage,
        const string& entryPoint,
        const ShaderCompilerOptions& options);
    
    // Run glslc compiler on file
    ShaderCompilationResult RunGlslcFromFile(
        const string& filePath,
        RHI::EShaderStage stage,
        const string& entryPoint,
        const ShaderCompilerOptions& options);
    
    // Map shader stage to glslc stage
    string MapShaderStage(RHI::EShaderStage stage) const;
    
    // Build glslc arguments
    vector<string> BuildGlslcArgs(
        RHI::EShaderStage stage,
        const string& entryPoint,
        const ShaderCompilerOptions& options) const;
};

// ============================================================================
// HLSL Compiler
// ============================================================================

// HLSL to SPIR-V compiler using dxc
class HLSLCompiler : public ShaderCompiler {
public:
    HLSLCompiler();
    ~HLSLCompiler() override;
    
    ShaderCompilationResult CompileShader(
        const string& source,
        RHI::EShaderStage stage,
        const string& entryPoint,
        RHI::EShaderLanguage language,
        const ShaderCompilerOptions& options = ShaderCompilerOptions()) override;
    
    ShaderCompilationResult CompileShaderFromFile(
        const string& filePath,
        RHI::EShaderStage stage,
        const string& entryPoint,
        RHI::EShaderLanguage language,
        const ShaderCompilerOptions& options = ShaderCompilerOptions()) override;
    
    vector<RHI::EShaderLanguage> GetSupportedLanguages() const override;
    vector<RHI::EShaderStage> GetSupportedStages(RHI::EShaderLanguage language) const override;
    
    bool ValidateSource(
        const string& source,
        RHI::EShaderLanguage language,
        string& errorMessage) override;
    
    string PreprocessSource(
        const string& source,
        const ShaderCompilerOptions& options,
        string& errorMessage) override;
    
    // Set dxc executable path
    void SetDxcPath(const string& path) { m_dxcPath = path; }
    
    // Get dxc executable path
    const string& GetDxcPath() const { return m_dxcPath; }
    
private:
    string m_dxcPath;
    bool m_initialized;
    
    // Run dxc compiler
    ShaderCompilationResult RunDxc(
        const string& source,
        RHI::EShaderStage stage,
        const string& entryPoint,
        const ShaderCompilerOptions& options);
    
    // Run dxc compiler on file
    ShaderCompilationResult RunDxcFromFile(
        const string& filePath,
        RHI::EShaderStage stage,
        const string& entryPoint,
        const ShaderCompilerOptions& options);
    
    // Map shader stage to dxc stage
    string MapShaderStage(RHI::EShaderStage stage) const;
    
    // Build dxc arguments
    vector<string> BuildDxcArgs(
        RHI::EShaderStage stage,
        const string& entryPoint,
        const ShaderCompilerOptions& options) const;
};

// ============================================================================
// Unified Shader Compiler
// ============================================================================

// Unified compiler that automatically selects the appropriate compiler
// based on the shader language
class UnifiedShaderCompiler : public ShaderCompiler {
public:
    UnifiedShaderCompiler();
    ~UnifiedShaderCompiler() override;
    
    ShaderCompilationResult CompileShader(
        const string& source,
        RHI::EShaderStage stage,
        const string& entryPoint,
        RHI::EShaderLanguage language,
        const ShaderCompilerOptions& options = ShaderCompilerOptions()) override;
    
    ShaderCompilationResult CompileShaderFromFile(
        const string& filePath,
        RHI::EShaderStage stage,
        const string& entryPoint,
        RHI::EShaderLanguage language,
        const ShaderCompilerOptions& options = ShaderCompilerOptions()) override;
    
    vector<RHI::EShaderLanguage> GetSupportedLanguages() const override;
    vector<RHI::EShaderStage> GetSupportedStages(RHI::EShaderLanguage language) const override;
    
    bool ValidateSource(
        const string& source,
        RHI::EShaderLanguage language,
        string& errorMessage) override;
    
    string PreprocessSource(
        const string& source,
        const ShaderCompilerOptions& options,
        string& errorMessage) override;
    
    // Set GLSL compiler
    void SetGLSLCompiler(GLSLCompiler* compiler) { m_glslCompiler = compiler; }
    
    // Set HLSL compiler
    void SetHLSLCompiler(HLSLCompiler* compiler) { m_hlslCompiler = compiler; }
    
    // Get GLSL compiler
    GLSLCompiler* GetGLSLCompiler() const { return m_glslCompiler; }
    
    // Get HLSL compiler
    HLSLCompiler* GetHLSLCompiler() const { return m_hlslCompiler; }
    
protected:
    GLSLCompiler* m_glslCompiler = nullptr;
    HLSLCompiler* m_hlslCompiler = nullptr;
    bool m_ownsCompilers = false;
    
    // Friend function for creating default compiler
    friend UnifiedShaderCompiler* CreateDefaultShaderCompiler();
};

// ============================================================================
// Shader Cache
// ============================================================================

// Shader compilation cache to avoid recompiling unchanged shaders
class ShaderCache {
public:
    static ShaderCache& GetInstance();
    
    // Get cached compilation result
    bool GetCachedResult(
        const string& sourceHash,
        ShaderCompilationResult& result) const;
    
    // Cache compilation result
    void CacheResult(
        const string& sourceHash,
        const ShaderCompilationResult& result);
    
    // Clear cache
    void Clear();
    
    // Set cache size limit
    void SetCacheSizeLimit(u64 sizeLimit) { m_cacheSizeLimit = sizeLimit; }
    
    // Get cache size
    u64 GetCacheSize() const { return m_cacheSize; }
    
private:
    ShaderCache() = default;
    ~ShaderCache();
    
    struct CacheEntry {
        ShaderCompilationResult* result;
        u64 size;
        u64 lastAccess;
    };
    
    unordered_map<string, CacheEntry> m_cache;
    u64 m_cacheSize = 0;
    u64 m_cacheSizeLimit = 1024 * 1024 * 100;  // 100 MB default
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Create default unified shader compiler
UnifiedShaderCompiler* CreateDefaultShaderCompiler();

// Compile shader using default compiler
ShaderCompilationResult CompileShader(
    const string& source,
    RHI::EShaderStage stage,
    const string& entryPoint,
    RHI::EShaderLanguage language = RHI::EShaderLanguage::GLSL,
    const ShaderCompilerOptions& options = ShaderCompilerOptions());

// Compile shader from file using default compiler
ShaderCompilationResult CompileShaderFromFile(
    const string& filePath,
    RHI::EShaderStage stage,
    const string& entryPoint,
    RHI::EShaderLanguage language = RHI::EShaderLanguage::GLSL,
    const ShaderCompilerOptions& options = ShaderCompilerOptions());

} // namespace Shader
} // namespace Luma