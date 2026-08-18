#pragma once

#include "Luma/RHI/RHIShader.h"

namespace Luma {
namespace RHI {

// Stub shader factory + shader + compiler. The deferred renderer's first
// stage (the RHI adapter wiring) never compiles a shader — the existing
// VulkanSceneView still owns scene rendering through the old path. When the
// next stage lands (GBuffer pass), the ShaderCompiler will be implemented
// against glslc (mirroring Engine/Rendering/Shader/ShaderCompiler).
class VulkanRHIShader final : public RHIShader {
public:
    explicit VulkanRHIShader(const ShaderDesc& desc) { m_desc = desc; }
};

class VulkanRHIShaderCompiler final : public RHIShaderCompiler {
public:
    VulkanRHIShaderCompiler() = default;
    ~VulkanRHIShaderCompiler() override = default;

    ShaderCompileResult CompileShader(const ShaderDesc& desc) override;
    RHIShader* CreateShaderFromBytecode(const ShaderBytecode& bytecode,
                                          const ShaderDesc& desc) override;
    void DestroyShader(RHIShader* shader) override;
    bool IsLanguageSupported(EShaderLanguage language) override;
    vector<const char*> GetSupportedProfiles(EShaderLanguage language) override;
};

class VulkanRHIShaderFactory final : public RHIShaderFactory {
public:
    VulkanRHIShaderFactory() : m_compiler(std::make_unique<VulkanRHIShaderCompiler>()) {}
    ~VulkanRHIShaderFactory() override = default;

    RHIShaderCompiler* GetShaderCompiler() override { return m_compiler.get(); }
    RHIShader* CreateShader(const ShaderDesc& desc) override;
    RHIShader* CreateShaderFromBytecode(const ShaderBytecode& bytecode,
                                          const ShaderDesc& desc) override;
    void DestroyShader(RHIShader* shader) override;

private:
    std::unique_ptr<VulkanRHIShaderCompiler> m_compiler;
};

}  // namespace RHI
}  // namespace Luma
