#include "Vulkan/RHI/VulkanRHIShader.h"

#include "Luma/Core/Log.h"

namespace Luma {
namespace RHI {

namespace {
const char* kStub = "VulkanRHIShaderFactory: shader compile not yet implemented";
}

ShaderCompileResult VulkanRHIShaderCompiler::CompileShader(const ShaderDesc& desc) {
    LUMA_LOG_WARN("RHI", "{} (CompileShader, stage={})", kStub,
                  static_cast<u32>(desc.stage));
    ShaderCompileResult r;
    r.success = false;
    r.errorMessage = kStub;
    return r;
}

RHIShader* VulkanRHIShaderCompiler::CreateShaderFromBytecode(
    const ShaderBytecode& /*bytecode*/, const ShaderDesc& desc) {
    // The next stage will use spirv-cross / direct spirv module creation here.
    LUMA_LOG_WARN("RHI", "{} (CreateShaderFromBytecode)", kStub);
    return new VulkanRHIShader(desc);
}

void VulkanRHIShaderCompiler::DestroyShader(RHIShader* shader) { delete shader; }

bool VulkanRHIShaderCompiler::IsLanguageSupported(EShaderLanguage language) {
    return language == EShaderLanguage::GLSL;
}

vector<const char*> VulkanRHIShaderCompiler::GetSupportedProfiles(
    EShaderLanguage /*language*/) {
    // No profiles yet; the next stage returns "vs_1_3"/"fs_1_3" etc.
    return {};
}

RHIShader* VulkanRHIShaderFactory::CreateShader(const ShaderDesc& desc) {
    auto* compiler = m_compiler.get();
    auto result = compiler->CompileShader(desc);
    if (!result.success) return nullptr;
    // The compile result holds the bytecode; if the next stage returns a
    // usable bytecode, we wrap it here. For now CompileShader always fails
    // and CreateShader returns nullptr.
    return nullptr;
}

RHIShader* VulkanRHIShaderFactory::CreateShaderFromBytecode(
    const ShaderBytecode& bytecode, const ShaderDesc& desc) {
    return m_compiler->CreateShaderFromBytecode(bytecode, desc);
}

void VulkanRHIShaderFactory::DestroyShader(RHIShader* shader) {
    m_compiler->DestroyShader(shader);
}

}  // namespace RHI
}  // namespace Luma
