#include "Luma/Shader/ShaderCompiler.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <unordered_map>

namespace Luma {
namespace Shader {

// ============================================================================
// GLSL Compiler
// ============================================================================

GLSLCompiler::GLSLCompiler()
    : m_initialized(false) {
    // Try to find glslc in common locations
    // TODO: Implement proper path discovery
    m_glslcPath = "glslc";
    m_initialized = true;
}

GLSLCompiler::~GLSLCompiler() {
}

ShaderCompilationResult GLSLCompiler::CompileShader(
    const string& source,
    RHI::EShaderStage stage,
    const string& entryPoint,
    RHI::EShaderLanguage language,
    const ShaderCompilerOptions& options) {
    
    if (language != RHI::EShaderLanguage::GLSL) {
        ShaderCompilationResult result;
        result.success = false;
        result.errorMessage = "GLSL compiler only supports GLSL language";
        return result;
    }
    
    return RunGlslc(source, stage, entryPoint, options);
}

ShaderCompilationResult GLSLCompiler::CompileShaderFromFile(
    const string& filePath,
    RHI::EShaderStage stage,
    const string& entryPoint,
    RHI::EShaderLanguage language,
    const ShaderCompilerOptions& options) {
    
    if (language != RHI::EShaderLanguage::GLSL) {
        ShaderCompilationResult result;
        result.success = false;
        result.errorMessage = "GLSL compiler only supports GLSL language";
        return result;
    }
    
    return RunGlslcFromFile(filePath, stage, entryPoint, options);
}

vector<RHI::EShaderLanguage> GLSLCompiler::GetSupportedLanguages() const {
    return {RHI::EShaderLanguage::GLSL};
}

vector<RHI::EShaderStage> GLSLCompiler::GetSupportedStages(RHI::EShaderLanguage language) const {
    if (language == RHI::EShaderLanguage::GLSL) {
        return {
            RHI::EShaderStage::Vertex,
            RHI::EShaderStage::Fragment,
            RHI::EShaderStage::Geometry,
            RHI::EShaderStage::TessControl,
            RHI::EShaderStage::TessEvaluation,
            RHI::EShaderStage::Compute
        };
    }
    return {};
}

bool GLSLCompiler::ValidateSource(
    const string& source,
    RHI::EShaderLanguage language,
    string& errorMessage) {
    
    if (language != RHI::EShaderLanguage::GLSL) {
        errorMessage = "GLSL compiler only supports GLSL language";
        return false;
    }
    
    // TODO: Implement proper validation
    // For now, just check if source is not empty
    if (source.empty()) {
        errorMessage = "Source is empty";
        return false;
    }
    
    return true;
}

string GLSLCompiler::PreprocessSource(
    const string& source,
    const ShaderCompilerOptions& options,
    string& errorMessage) {
    
    (void)source;
    (void)options;
    (void)errorMessage;
    // TODO: Implement proper preprocessing
    // For now, just return the source as-is
    return source;
}

ShaderCompilationResult GLSLCompiler::RunGlslc(
    const string& source,
    RHI::EShaderStage stage,
    const string& entryPoint,
    const ShaderCompilerOptions& options) {
    
    (void)source;
    (void)stage;
    (void)entryPoint;
    (void)options;
    
    ShaderCompilationResult result;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // TODO: Implement actual glslc execution
    // For now, return a stub result
    result.success = false;
    result.errorMessage = "GLSL compiler execution not yet implemented";
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.compilationTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    return result;
}

ShaderCompilationResult GLSLCompiler::RunGlslcFromFile(
    const string& filePath,
    RHI::EShaderStage stage,
    const string& entryPoint,
    const ShaderCompilerOptions& options) {
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        ShaderCompilationResult result;
        result.success = false;
        result.errorMessage = "Failed to open file: " + filePath;
        return result;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    string source = buffer.str();
    
    return RunGlslc(source, stage, entryPoint, options);
}

string GLSLCompiler::MapShaderStage(RHI::EShaderStage stage) const {
    switch (stage) {
        case RHI::EShaderStage::Vertex:
            return "vert";
        case RHI::EShaderStage::Fragment:
            return "frag";
        case RHI::EShaderStage::Geometry:
            return "geom";
        case RHI::EShaderStage::TessControl:
            return "tesc";
        case RHI::EShaderStage::TessEvaluation:
            return "tese";
        case RHI::EShaderStage::Compute:
            return "comp";
        default:
            return "";
    }
}

vector<string> GLSLCompiler::BuildGlslcArgs(
    RHI::EShaderStage stage,
    const string& entryPoint,
    const ShaderCompilerOptions& options) const {
    
    (void)stage;  // Stage is used in MapShaderStage, but the mapping function is not called yet
    
    vector<string> args;
    
    // Target environment
    args.push_back("--target-env=vulkan1.3");
    
    // Entry point
    args.push_back("-D" + entryPoint);
    
    // Optimization level
    switch (options.optimizationLevel) {
        case EShaderOptimizationLevel::None:
            args.push_back("-O0");
            break;
        case EShaderOptimizationLevel::O0:
            args.push_back("-O0");
            break;
        case EShaderOptimizationLevel::O1:
            args.push_back("-O1");
            break;
        case EShaderOptimizationLevel::O2:
            args.push_back("-O2");
            break;
        case EShaderOptimizationLevel::O3:
            args.push_back("-O3");
            break;
    }
    
    // Debug info
    if (options.debugInfo) {
        args.push_back("-g");
    }
    
    // SPIR-V version
    if (options.spirvVersion14) {
        args.push_back("--target-spv=spv1.4");
    }
    
    // Defines
    for (const auto& define : options.defines) {
        args.push_back("-D" + define);
    }
    
    // Include directory
    if (!options.includeDirectory.empty()) {
        args.push_back("-I" + options.includeDirectory);
    }
    
    return args;
}

// ============================================================================
// HLSL Compiler
// ============================================================================

HLSLCompiler::HLSLCompiler()
    : m_initialized(false) {
    // Try to find dxc in common locations
    // TODO: Implement proper path discovery
    m_dxcPath = "dxc";
    m_initialized = true;
}

HLSLCompiler::~HLSLCompiler() {
}

ShaderCompilationResult HLSLCompiler::CompileShader(
    const string& source,
    RHI::EShaderStage stage,
    const string& entryPoint,
    RHI::EShaderLanguage language,
    const ShaderCompilerOptions& options) {
    
    if (language != RHI::EShaderLanguage::HLSL) {
        ShaderCompilationResult result;
        result.success = false;
        result.errorMessage = "HLSL compiler only supports HLSL language";
        return result;
    }
    
    return RunDxc(source, stage, entryPoint, options);
}

ShaderCompilationResult HLSLCompiler::CompileShaderFromFile(
    const string& filePath,
    RHI::EShaderStage stage,
    const string& entryPoint,
    RHI::EShaderLanguage language,
    const ShaderCompilerOptions& options) {
    
    if (language != RHI::EShaderLanguage::HLSL) {
        ShaderCompilationResult result;
        result.success = false;
        result.errorMessage = "HLSL compiler only supports HLSL language";
        return result;
    }
    
    return RunDxcFromFile(filePath, stage, entryPoint, options);
}

vector<RHI::EShaderLanguage> HLSLCompiler::GetSupportedLanguages() const {
    return {RHI::EShaderLanguage::HLSL};
}

vector<RHI::EShaderStage> HLSLCompiler::GetSupportedStages(RHI::EShaderLanguage language) const {
    if (language == RHI::EShaderLanguage::HLSL) {
        return {
            RHI::EShaderStage::Vertex,
            RHI::EShaderStage::Fragment,
            RHI::EShaderStage::Geometry,
            RHI::EShaderStage::TessControl,
            RHI::EShaderStage::TessEvaluation,
            RHI::EShaderStage::Compute
        };
    }
    return {};
}

bool HLSLCompiler::ValidateSource(
    const string& source,
    RHI::EShaderLanguage language,
    string& errorMessage) {
    
    if (language != RHI::EShaderLanguage::HLSL) {
        errorMessage = "HLSL compiler only supports HLSL language";
        return false;
    }
    
    // TODO: Implement proper validation
    if (source.empty()) {
        errorMessage = "Source is empty";
        return false;
    }
    
    return true;
}

string HLSLCompiler::PreprocessSource(
    const string& source,
    const ShaderCompilerOptions& options,
    string& errorMessage) {
    
    (void)source;
    (void)options;
    (void)errorMessage;
    // TODO: Implement proper preprocessing
    return source;
}

ShaderCompilationResult HLSLCompiler::RunDxc(
    const string& source,
    RHI::EShaderStage stage,
    const string& entryPoint,
    const ShaderCompilerOptions& options) {
    
    (void)source;
    (void)stage;
    (void)entryPoint;
    (void)options;
    
    ShaderCompilationResult result;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // TODO: Implement actual dxc execution
    // For now, return a stub result
    result.success = false;
    result.errorMessage = "HLSL compiler execution not yet implemented";
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.compilationTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    return result;
}

ShaderCompilationResult HLSLCompiler::RunDxcFromFile(
    const string& filePath,
    RHI::EShaderStage stage,
    const string& entryPoint,
    const ShaderCompilerOptions& options) {
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        ShaderCompilationResult result;
        result.success = false;
        result.errorMessage = "Failed to open file: " + filePath;
        return result;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    string source = buffer.str();
    
    return RunDxc(source, stage, entryPoint, options);
}

string HLSLCompiler::MapShaderStage(RHI::EShaderStage stage) const {
    switch (stage) {
        case RHI::EShaderStage::Vertex:
            return "vs";
        case RHI::EShaderStage::Fragment:
            return "ps";
        case RHI::EShaderStage::Geometry:
            return "gs";
        case RHI::EShaderStage::TessControl:
            return "hs";
        case RHI::EShaderStage::TessEvaluation:
            return "ds";
        case RHI::EShaderStage::Compute:
            return "cs";
        default:
            return "";
    }
}

vector<string> HLSLCompiler::BuildDxcArgs(
    RHI::EShaderStage stage,
    const string& entryPoint,
    const ShaderCompilerOptions& options) const {
    
    (void)stage;  // Stage is used in MapShaderStage, but the mapping function is not called yet
    
    vector<string> args;
    
    // Target profile
    string stageStr = MapShaderStage(stage);
    if (!options.hlslProfile.empty()) {
        args.push_back("-T" + options.hlslProfile);
    } else {
        args.push_back("-T" + stageStr + "_6_0");
    }
    
    // Entry point
    args.push_back("-E" + entryPoint);
    
    // Target SPIR-V
    args.push_back("-spirv");
    
    // Optimization level
    switch (options.optimizationLevel) {
        case EShaderOptimizationLevel::None:
            args.push_back("-Od");
            break;
        case EShaderOptimizationLevel::O0:
            args.push_back("-O0");
            break;
        case EShaderOptimizationLevel::O1:
            args.push_back("-O1");
            break;
        case EShaderOptimizationLevel::O2:
            args.push_back("-O2");
            break;
        case EShaderOptimizationLevel::O3:
            args.push_back("-O3");
            break;
    }
    
    // Debug info
    if (options.debugInfo) {
        args.push_back("-Zi");
    }
    
    // 16-bit types
    if (options.hlslEnable16bitTypes) {
        args.push_back("-enable-16bit-types");
    }
    
    // Defines
    for (const auto& define : options.defines) {
        args.push_back("-D" + define);
    }
    
    // Include directory
    if (!options.includeDirectory.empty()) {
        args.push_back("-I" + options.includeDirectory);
    }
    
    return args;
}

// ============================================================================
// Unified Shader Compiler
// ============================================================================

UnifiedShaderCompiler::UnifiedShaderCompiler()
    : m_ownsCompilers(false) {
}

UnifiedShaderCompiler::~UnifiedShaderCompiler() {
    if (m_ownsCompilers) {
        delete m_glslCompiler;
        delete m_hlslCompiler;
    }
}

ShaderCompilationResult UnifiedShaderCompiler::CompileShader(
    const string& source,
    RHI::EShaderStage stage,
    const string& entryPoint,
    RHI::EShaderLanguage language,
    const ShaderCompilerOptions& options) {
    
    switch (language) {
        case RHI::EShaderLanguage::GLSL:
            if (m_glslCompiler) {
                return m_glslCompiler->CompileShader(source, stage, entryPoint, language, options);
            }
            break;
        case RHI::EShaderLanguage::HLSL:
            if (m_hlslCompiler) {
                return m_hlslCompiler->CompileShader(source, stage, entryPoint, language, options);
            }
            break;
        default:
            break;
    }
    
    ShaderCompilationResult result;
    result.success = false;
    result.errorMessage = "No compiler available for language";
    return result;
}

ShaderCompilationResult UnifiedShaderCompiler::CompileShaderFromFile(
    const string& filePath,
    RHI::EShaderStage stage,
    const string& entryPoint,
    RHI::EShaderLanguage language,
    const ShaderCompilerOptions& options) {
    
    switch (language) {
        case RHI::EShaderLanguage::GLSL:
            if (m_glslCompiler) {
                return m_glslCompiler->CompileShaderFromFile(filePath, stage, entryPoint, language, options);
            }
            break;
        case RHI::EShaderLanguage::HLSL:
            if (m_hlslCompiler) {
                return m_hlslCompiler->CompileShaderFromFile(filePath, stage, entryPoint, language, options);
            }
            break;
        default:
            break;
    }
    
    ShaderCompilationResult result;
    result.success = false;
    result.errorMessage = "No compiler available for language";
    return result;
}

vector<RHI::EShaderLanguage> UnifiedShaderCompiler::GetSupportedLanguages() const {
    vector<RHI::EShaderLanguage> languages;
    
    if (m_glslCompiler) {
        auto glslLangs = m_glslCompiler->GetSupportedLanguages();
        languages.insert(languages.end(), glslLangs.begin(), glslLangs.end());
    }
    
    if (m_hlslCompiler) {
        auto hlslLangs = m_hlslCompiler->GetSupportedLanguages();
        languages.insert(languages.end(), hlslLangs.begin(), hlslLangs.end());
    }
    
    return languages;
}

vector<RHI::EShaderStage> UnifiedShaderCompiler::GetSupportedStages(RHI::EShaderLanguage language) const {
    switch (language) {
        case RHI::EShaderLanguage::GLSL:
            if (m_glslCompiler) {
                return m_glslCompiler->GetSupportedStages(language);
            }
            break;
        case RHI::EShaderLanguage::HLSL:
            if (m_hlslCompiler) {
                return m_hlslCompiler->GetSupportedStages(language);
            }
            break;
        default:
            break;
    }
    return {};
}

bool UnifiedShaderCompiler::ValidateSource(
    const string& source,
    RHI::EShaderLanguage language,
    string& errorMessage) {
    
    switch (language) {
        case RHI::EShaderLanguage::GLSL:
            if (m_glslCompiler) {
                return m_glslCompiler->ValidateSource(source, language, errorMessage);
            }
            break;
        case RHI::EShaderLanguage::HLSL:
            if (m_hlslCompiler) {
                return m_hlslCompiler->ValidateSource(source, language, errorMessage);
            }
            break;
        default:
            break;
    }
    
    errorMessage = "No compiler available for language";
    return false;
}

string UnifiedShaderCompiler::PreprocessSource(
    const string& source,
    const ShaderCompilerOptions& options,
    string& errorMessage) {
    
    // TODO: Implement unified preprocessing
    (void)source;
    (void)options;
    (void)errorMessage;
    return source;
}

// ============================================================================
// Shader Cache
// ============================================================================

ShaderCache& ShaderCache::GetInstance() {
    static ShaderCache instance;
    return instance;
}

ShaderCache::~ShaderCache() {
    Clear();
}

bool ShaderCache::GetCachedResult(
    const string& sourceHash,
    ShaderCompilationResult& result) const {
    
    auto it = m_cache.find(sourceHash);
    if (it != m_cache.end()) {
        // Copy the result
        result.success = it->second.result->success;
        result.errorMessage = it->second.result->errorMessage;
        result.warningMessage = it->second.result->warningMessage;
        result.compilationTimeMs = it->second.result->compilationTimeMs;
        result.sourceMappings = it->second.result->sourceMappings;
        
        // Copy bytecode
        result.bytecode.data = it->second.result->bytecode.data;
        result.bytecode.size = it->second.result->bytecode.size;
        result.bytecode.ownsData = false;
        
        return true;
    }
    return false;
}

void ShaderCache::CacheResult(
    const string& sourceHash,
    const ShaderCompilationResult& result) {
    
    // Create a copy of the result
    auto* resultCopy = new ShaderCompilationResult();
    resultCopy->success = result.success;
    resultCopy->errorMessage = result.errorMessage;
    resultCopy->warningMessage = result.warningMessage;
    resultCopy->compilationTimeMs = result.compilationTimeMs;
    resultCopy->sourceMappings = result.sourceMappings;
    
    // Copy bytecode (need to handle move semantics)
    resultCopy->bytecode.data = result.bytecode.data;
    resultCopy->bytecode.size = result.bytecode.size;
    resultCopy->bytecode.ownsData = false;  // Don't take ownership
    
    CacheEntry entry;
    entry.result = resultCopy;
    entry.size = result.bytecode.size;
    entry.lastAccess = 0;  // TODO: Use actual timestamp
    
    m_cache[sourceHash] = entry;
    m_cacheSize += entry.size;
    
    // Check cache size limit
    if (m_cacheSize > m_cacheSizeLimit) {
        // TODO: Implement cache eviction
    }
}

void ShaderCache::Clear() {
    for (auto& [hash, entry] : m_cache) {
        if (entry.result) {
            delete entry.result;
        }
    }
    m_cache.clear();
    m_cacheSize = 0;
}

// ============================================================================
// Convenience Functions
// ============================================================================

UnifiedShaderCompiler* CreateDefaultShaderCompiler() {
    auto* compiler = new UnifiedShaderCompiler();
    compiler->m_glslCompiler = new GLSLCompiler();
    compiler->m_hlslCompiler = new HLSLCompiler();
    compiler->m_ownsCompilers = true;
    return compiler;
}

ShaderCompilationResult CompileShader(
    const string& source,
    RHI::EShaderStage stage,
    const string& entryPoint,
    RHI::EShaderLanguage language,
    const ShaderCompilerOptions& options) {
    
    static auto* compiler = CreateDefaultShaderCompiler();
    return compiler->CompileShader(source, stage, entryPoint, language, options);
}

ShaderCompilationResult CompileShaderFromFile(
    const string& filePath,
    RHI::EShaderStage stage,
    const string& entryPoint,
    RHI::EShaderLanguage language,
    const ShaderCompilerOptions& options) {
    
    static auto* compiler = CreateDefaultShaderCompiler();
    return compiler->CompileShaderFromFile(filePath, stage, entryPoint, language, options);
}

} // namespace Shader
} // namespace Luma