#include "Luma/Shader/GlobalShader.h"

namespace Luma {
namespace Shader {

// ============================================================================
// Global Shader Base Class
// ============================================================================

GlobalShader::GlobalShader(EGlobalShaderType type, const string& name)
    : m_name(name), m_type(type) {
}

void GlobalShader::SetPermutation(const string& permutation) {
    m_currentPermutation = permutation;
}

// ============================================================================
// Global Shader Registry
// ============================================================================

GlobalShaderRegistry& GlobalShaderRegistry::GetInstance() {
    static GlobalShaderRegistry instance;
    return instance;
}

GlobalShaderRegistry::~GlobalShaderRegistry() {
    ReleaseAll();
}

void GlobalShaderRegistry::RegisterShader(EGlobalShaderType type, GlobalShader* shader) {
    m_shaders[type] = shader;
}

void GlobalShaderRegistry::UnregisterShader(EGlobalShaderType type) {
    m_shaders.erase(type);
}

GlobalShader* GlobalShaderRegistry::GetShader(EGlobalShaderType type) const {
    auto it = m_shaders.find(type);
    if (it != m_shaders.end()) {
        return it->second;
    }
    return nullptr;
}

bool GlobalShaderRegistry::InitializeAll() {
    for (auto& [type, shader] : m_shaders) {
        if (!shader->Initialize()) {
            return false;
        }
    }
    return true;
}

void GlobalShaderRegistry::ReleaseAll() {
    for (auto& [type, shader] : m_shaders) {
        shader->Release();
    }
}

void GlobalShaderRegistry::Clear() {
    ReleaseAll();
    m_shaders.clear();
}

// ============================================================================
// Common Global Shaders
// ============================================================================

ToneMappingShader::ToneMappingShader()
    : GlobalShader(EGlobalShaderType::ToneMapping, "ToneMapping") {
}

bool ToneMappingShader::Initialize() {
    // TODO: Load and compile tone mapping shader
    // This will be implemented when the full shader system is ready
    m_initialized = true;
    return true;
}

void ToneMappingShader::Release() {
    m_initialized = false;
}

BloomShader::BloomShader()
    : GlobalShader(EGlobalShaderType::Bloom, "Bloom") {
}

bool BloomShader::Initialize() {
    // TODO: Load and compile bloom shader
    m_initialized = true;
    return true;
}

void BloomShader::Release() {
    m_initialized = false;
}

CopyTextureShader::CopyTextureShader()
    : GlobalShader(EGlobalShaderType::CopyTexture, "CopyTexture") {
}

bool CopyTextureShader::Initialize() {
    // TODO: Load and compile copy texture shader
    m_initialized = true;
    return true;
}

void CopyTextureShader::Release() {
    m_initialized = false;
}

GenerateMipsShader::GenerateMipsShader()
    : GlobalShader(EGlobalShaderType::GenerateMips, "GenerateMips") {
}

bool GenerateMipsShader::Initialize() {
    // TODO: Load and compile generate mips shader
    m_initialized = true;
    return true;
}

void GenerateMipsShader::Release() {
    m_initialized = false;
}

ClearTextureShader::ClearTextureShader()
    : GlobalShader(EGlobalShaderType::ClearTexture, "ClearTexture") {
}

bool ClearTextureShader::Initialize() {
    // TODO: Load and compile clear texture shader
    m_initialized = true;
    return true;
}

void ClearTextureShader::Release() {
    m_initialized = false;
}

WireframeShader::WireframeShader()
    : GlobalShader(EGlobalShaderType::Wireframe, "Wireframe") {
}

bool WireframeShader::Initialize() {
    // TODO: Load and compile wireframe shader
    m_initialized = true;
    return true;
}

void WireframeShader::Release() {
    m_initialized = false;
}

NormalsShader::NormalsShader()
    : GlobalShader(EGlobalShaderType::Normals, "Normals") {
}

bool NormalsShader::Initialize() {
    // TODO: Load and compile normals shader
    m_initialized = true;
    return true;
}

void NormalsShader::Release() {
    m_initialized = false;
}

DepthVisualizationShader::DepthVisualizationShader()
    : GlobalShader(EGlobalShaderType::DepthVisualization, "DepthVisualization") {
}

bool DepthVisualizationShader::Initialize() {
    // TODO: Load and compile depth visualization shader
    m_initialized = true;
    return true;
}

void DepthVisualizationShader::Release() {
    m_initialized = false;
}

// ============================================================================
// Global Shader Manager
// ============================================================================

GlobalShaderManager& GlobalShaderManager::GetInstance() {
    static GlobalShaderManager instance;
    return instance;
}

GlobalShaderManager::~GlobalShaderManager() {
    Release();
}

bool GlobalShaderManager::Initialize() {
    if (m_initialized) {
        return true;
    }
    
    // Register all global shaders
    auto& registry = GlobalShaderRegistry::GetInstance();
    
    registry.RegisterShader(EGlobalShaderType::ToneMapping, new ToneMappingShader());
    registry.RegisterShader(EGlobalShaderType::Bloom, new BloomShader());
    registry.RegisterShader(EGlobalShaderType::CopyTexture, new CopyTextureShader());
    registry.RegisterShader(EGlobalShaderType::GenerateMips, new GenerateMipsShader());
    registry.RegisterShader(EGlobalShaderType::ClearTexture, new ClearTextureShader());
    registry.RegisterShader(EGlobalShaderType::Wireframe, new WireframeShader());
    registry.RegisterShader(EGlobalShaderType::Normals, new NormalsShader());
    registry.RegisterShader(EGlobalShaderType::DepthVisualization, new DepthVisualizationShader());
    
    // Initialize all shaders
    m_initialized = registry.InitializeAll();
    return m_initialized;
}

void GlobalShaderManager::Release() {
    if (!m_initialized) {
        return;
    }
    
    GlobalShaderRegistry::GetInstance().ReleaseAll();
    m_initialized = false;
}

bool GlobalShaderManager::ReloadAll() {
    Release();
    return Initialize();
}

} // namespace Shader
} // namespace Luma