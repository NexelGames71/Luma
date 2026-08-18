#pragma once

#include <memory>
#include <vector>
#include <string>

#include "Luma/Core/Types.h"
#include "Luma/RHI/RHITypes.h"

// RHI shader interfaces. Inspired by UE5's shader system but simplified for
// Luma's architecture. Supports both GLSL and HLSL compilation to SPIR-V.

namespace Luma {
namespace RHI {

using std::vector;
using std::string;

// Forward declarations
class RHIDevice;

// ============================================================================
// Shader Bytecode
// ============================================================================

// Shader bytecode container
struct ShaderBytecode {
    const void* data = nullptr;
    u64 size = 0;
    
    // Ownership flag - if true, we own the data and should free it
    bool ownsData = false;
    
    ShaderBytecode() = default;
    explicit ShaderBytecode(const void* data_, u64 size_, bool ownsData_ = false)
        : data(data_), size(size_), ownsData(ownsData_) {}
    
    ~ShaderBytecode() {
        if (ownsData && data) {
            delete[] static_cast<const u8*>(data);
        }
    }
    
    // Move constructor
    ShaderBytecode(ShaderBytecode&& other) noexcept
        : data(other.data), size(other.size), ownsData(other.ownsData) {
        other.data = nullptr;
        other.size = 0;
        other.ownsData = false;
    }
    
    // Move assignment
    ShaderBytecode& operator=(ShaderBytecode&& other) noexcept {
        if (this != &other) {
            if (ownsData && data) {
                delete[] static_cast<const u8*>(data);
            }
            data = other.data;
            size = other.size;
            ownsData = other.ownsData;
            other.data = nullptr;
            other.size = 0;
            other.ownsData = false;
        }
        return *this;
    }
    
    // Disable copy
    ShaderBytecode(const ShaderBytecode&) = delete;
    ShaderBytecode& operator=(const ShaderBytecode&) = delete;
};

// ============================================================================
// Shader Description
// ============================================================================

// Shader entry point description
struct ShaderEntryPoint {
    const char* name = "main";
    EShaderStage stage = EShaderStage::Vertex;
};

// Shader compilation target
struct ShaderTarget {
    EShaderLanguage language = EShaderLanguage::GLSL;
    const char* profile = nullptr;  // e.g., "vs_5_0", "ps_5_0" for HLSL
};

// Shader compilation flags
enum class EShaderCompileFlags : u32 {
    None = 0,
    Debug = 1 << 0,              // Generate debug information
    Optimize = 1 << 1,          // Enable optimization
    O0 = 1 << 2,                // No optimization
    O1 = 1 << 3,                // Basic optimization
    O2 = 1 << 4,                // Standard optimization
    O3 = 1 << 5,                // Aggressive optimization
    PackMatrixRowMajor = 1 << 6, // Row-major matrix packing
    PackMatrixColumnMajor = 1 << 7, // Column-major matrix packing
};
inline EShaderCompileFlags operator|(EShaderCompileFlags a, EShaderCompileFlags b) {
    return static_cast<EShaderCompileFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline EShaderCompileFlags operator&(EShaderCompileFlags a, EShaderCompileFlags b) {
    return static_cast<EShaderCompileFlags>(static_cast<u32>(a) & static_cast<u32>(b));
}

// Shader description
struct ShaderDesc {
    const char* source = nullptr;      // Shader source code
    u64 sourceSize = 0;                // Size of source code
    const char* entryPoint = "main";   // Entry point function name
    EShaderStage stage = EShaderStage::Vertex;
    EShaderLanguage language = EShaderLanguage::GLSL;
    EShaderCompileFlags flags = EShaderCompileFlags::Optimize;
    
    // HLSL target profile (e.g., "vs_5_0", "ps_5_0")
    const char* targetProfile = nullptr;
    
    // Include directories for shader compilation
    const char** includeDirs = nullptr;
    u32 numIncludeDirs = 0;
    
    // Preprocessor definitions
    const char** defines = nullptr;
    u32 numDefines = 0;
    
    // Debug name
    string name;
};

// ============================================================================
// Shader Interface
// ============================================================================

// Shader reflection information
struct ShaderReflection {
    // Resource bindings
    struct ResourceBinding {
        u32 set = 0;
        u32 binding = 0;
        const char* name = nullptr;
        EShaderStage stages = EShaderStage::Vertex;
    };
    
    vector<ResourceBinding> resourceBindings;
    
    // Push constant ranges
    struct PushConstantRange {
        u32 offset = 0;
        u32 size = 0;
        EShaderStage stages = EShaderStage::Vertex;
    };
    
    vector<PushConstantRange> pushConstantRanges;
    
    // Input attributes (for vertex shaders)
    struct InputAttribute {
        u32 location = 0;
        const char* name = nullptr;
        ETextureFormat format = ETextureFormat::Unknown;
    };
    
    vector<InputAttribute> inputAttributes;
    
    // Required capabilities
    u32 requiredCapabilities = 0;
};

// Shader interface
class RHIShader {
public:
    virtual ~RHIShader() = default;
    
    // Get shader description
    const ShaderDesc& GetDesc() const { return m_desc; }
    
    // Get shader bytecode
    const ShaderBytecode& GetBytecode() const { return m_bytecode; }
    
    // Get shader reflection
    const ShaderReflection& GetReflection() const { return m_reflection; }
    
    // Get shader stage
    EShaderStage GetStage() const { return m_desc.stage; }
    
    // Get debug name
    const std::string& GetName() const { return m_desc.name; }
    
protected:
    ShaderDesc m_desc;
    ShaderBytecode m_bytecode;
    ShaderReflection m_reflection;
};

// ============================================================================
// Shader Compiler Interface
// ============================================================================

// Shader compilation result
struct ShaderCompileResult {
    bool success = false;
    string errorMessage;
    ShaderBytecode bytecode;
    ShaderReflection reflection;
};

// Shader compiler interface (implemented by backends)
class RHIShaderCompiler {
public:
    virtual ~RHIShaderCompiler() = default;
    
    // Compile shader from source
    virtual ShaderCompileResult CompileShader(const ShaderDesc& desc) = 0;
    
    // Compile shader from bytecode (for pre-compiled shaders)
    virtual RHIShader* CreateShaderFromBytecode(const ShaderBytecode& bytecode, const ShaderDesc& desc) = 0;
    
    // Destroy shader
    virtual void DestroyShader(RHIShader* shader) = 0;
    
    // Get supported shader languages
    virtual bool IsLanguageSupported(EShaderLanguage language) = 0;
    
    // Get supported shader profiles for a language
    virtual vector<const char*> GetSupportedProfiles(EShaderLanguage language) = 0;
};

// ============================================================================
// Shader Factory
// ============================================================================

// Shader creation interface (implemented by RHI backends)
class RHIShaderFactory {
public:
    virtual ~RHIShaderFactory() = default;
    
    // Get shader compiler
    virtual RHIShaderCompiler* GetShaderCompiler() = 0;
    
    // Create shader from description (compiles shader)
    virtual RHIShader* CreateShader(const ShaderDesc& desc) = 0;
    
    // Create shader from bytecode (for pre-compiled shaders)
    virtual RHIShader* CreateShaderFromBytecode(const ShaderBytecode& bytecode, const ShaderDesc& desc) = 0;
    
    // Destroy shader
    virtual void DestroyShader(RHIShader* shader) = 0;
};

} // namespace RHI
} // namespace Luma