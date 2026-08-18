#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Luma/Core/Types.h"
#include "Luma/Shader/Shader.h"

// Global shader system. Inspired by UE5's global shader system but adapted for
// Luma's simpler architecture. Provides a way to define and manage global shaders
// that are used across the entire rendering pipeline.

namespace Luma {
namespace Shader {

using std::string;
using std::vector;
using std::unordered_map;

// ============================================================================
// Global Shader Types
// ============================================================================

// Global shader type
enum class EGlobalShaderType : u32 {
    // Post-processing shaders
    ToneMapping,
    Bloom,
    MotionBlur,
    DepthOfField,
    AntiAliasing,
    
    // Utility shaders
    CopyTexture,
    GenerateMips,
    ClearTexture,
    Downsample,
    Upsample,
    
    // Lighting shaders
    DirectionalLight,
    PointLight,
    SpotLight,
    AmbientLight,
    
    // Shadow shaders
    ShadowMapGen,
    ShadowMapFilter,
    CascadedShadowMap,
    
    // Debug shaders
    Wireframe,
    Normals,
    Tangents,
    DepthVisualization,
    
    // Custom
    Custom,
};

// ============================================================================
// Global Shader Base Class
// ============================================================================

// Base class for global shaders
class GlobalShader {
public:
    virtual ~GlobalShader() = default;
    
    // Get shader name
    const string& GetName() const { return m_name; }
    
    // Get shader type
    EGlobalShaderType GetType() const { return m_type; }
    
    // Get shader program
    ShaderProgram* GetProgram() const { return m_program; }
    
    // Check if shader is initialized
    bool IsInitialized() const { return m_initialized; }
    
    // Initialize shader
    virtual bool Initialize() = 0;
    
    // Release shader
    virtual void Release() = 0;
    
    // Get shader permutations
    const vector<string>& GetPermutations() const { return m_permutations; }
    
    // Set shader permutation
    void SetPermutation(const string& permutation);
    
    // Get current permutation
    const string& GetCurrentPermutation() const { return m_currentPermutation; }
    
protected:
    GlobalShader(EGlobalShaderType type, const string& name);
    
    string m_name;
    EGlobalShaderType m_type;
    ShaderProgram* m_program = nullptr;
    bool m_initialized = false;
    vector<string> m_permutations;
    string m_currentPermutation;
};

// ============================================================================
// Global Shader Registry
// ============================================================================

// Global registry for all global shaders
class GlobalShaderRegistry {
public:
    static GlobalShaderRegistry& GetInstance();
    
    // Register global shader
    void RegisterShader(EGlobalShaderType type, GlobalShader* shader);
    
    // Unregister global shader
    void UnregisterShader(EGlobalShaderType type);
    
    // Get global shader by type
    GlobalShader* GetShader(EGlobalShaderType type) const;
    
    // Get all global shaders
    const unordered_map<EGlobalShaderType, GlobalShader*>& GetAllShaders() const { return m_shaders; }
    
    // Initialize all global shaders
    bool InitializeAll();
    
    // Release all global shaders
    void ReleaseAll();
    
    // Clear all global shaders
    void Clear();
    
private:
    GlobalShaderRegistry() = default;
    ~GlobalShaderRegistry();
    
    unordered_map<EGlobalShaderType, GlobalShader*> m_shaders;
};

// ============================================================================
// Common Global Shaders
// ============================================================================

// Tone mapping shader
class ToneMappingShader : public GlobalShader {
public:
    ToneMappingShader();
    ~ToneMappingShader() override = default;
    
    bool Initialize() override;
    void Release() override;
};

// Bloom shader
class BloomShader : public GlobalShader {
public:
    BloomShader();
    ~BloomShader() override = default;
    
    bool Initialize() override;
    void Release() override;
};

// Copy texture shader
class CopyTextureShader : public GlobalShader {
public:
    CopyTextureShader();
    ~CopyTextureShader() override = default;
    
    bool Initialize() override;
    void Release() override;
};

// Generate mips shader
class GenerateMipsShader : public GlobalShader {
public:
    GenerateMipsShader();
    ~GenerateMipsShader() override = default;
    
    bool Initialize() override;
    void Release() override;
};

// Clear texture shader
class ClearTextureShader : public GlobalShader {
public:
    ClearTextureShader();
    ~ClearTextureShader() override = default;
    
    bool Initialize() override;
    void Release() override;
};

// Wireframe shader
class WireframeShader : public GlobalShader {
public:
    WireframeShader();
    ~WireframeShader() override = default;
    
    bool Initialize() override;
    void Release() override;
};

// Normals visualization shader
class NormalsShader : public GlobalShader {
public:
    NormalsShader();
    ~NormalsShader() override = default;
    
    bool Initialize() override;
    void Release() override;
};

// Depth visualization shader
class DepthVisualizationShader : public GlobalShader {
public:
    DepthVisualizationShader();
    ~DepthVisualizationShader() override = default;
    
    bool Initialize() override;
    void Release() override;
};

// ============================================================================
// Global Shader Manager
// ============================================================================

// Manager for global shader initialization and lifecycle
class GlobalShaderManager {
public:
    static GlobalShaderManager& GetInstance();
    
    // Initialize all global shaders
    bool Initialize();
    
    // Release all global shaders
    void Release();
    
    // Check if initialized
    bool IsInitialized() const { return m_initialized; }
    
    // Reload all global shaders (for hot-reloading)
    bool ReloadAll();
    
private:
    GlobalShaderManager() = default;
    ~GlobalShaderManager();
    
    bool m_initialized = false;
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Get global shader by type
inline GlobalShader* GetGlobalShader(EGlobalShaderType type) {
    return GlobalShaderRegistry::GetInstance().GetShader(type);
}

// Initialize global shaders
inline bool InitializeGlobalShaders() {
    return GlobalShaderManager::GetInstance().Initialize();
}

// Release global shaders
inline void ReleaseGlobalShaders() {
    GlobalShaderManager::GetInstance().Release();
}

} // namespace Shader
} // namespace Luma