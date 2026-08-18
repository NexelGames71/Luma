#include "Vulkan/RHI/VulkanRHIPipeline.h"

#include "Luma/Core/Log.h"

namespace Luma {
namespace RHI {

namespace {
const char* kStub =
    "VulkanRHIPipelineStateFactory: pipeline state not yet implemented";
void LogStub(const char* method) {
    LUMA_LOG_WARN("RHI", "{} ({})", kStub, method);
}
}

// Minimal concrete state subclasses. They simply hold the desc; no Vulkan
// handle is allocated. The deferred renderer's first stage does not call
// any of these â€” they exist only to let the factory compile.

class VulkanRHIRasterizerState final : public RHIRasterizerState {
public:
    explicit VulkanRHIRasterizerState(const RasterizerDesc& desc) { m_desc = desc; }
};

class VulkanRHIBlendState final : public RHIBlendState {
public:
    explicit VulkanRHIBlendState(const BlendDesc& desc) { m_desc = desc; }
};

class VulkanRHIDepthStencilState final : public RHIDepthStencilState {
public:
    explicit VulkanRHIDepthStencilState(const DepthStencilDesc& desc) { m_desc = desc; }
};

class VulkanRHIInputLayout final : public RHIInputLayout {
public:
    explicit VulkanRHIInputLayout(const InputLayoutDesc& desc) { m_desc = desc; }
};

class VulkanRHIGraphicsPipelineState final : public RHIGraphicsPipelineState {
public:
    explicit VulkanRHIGraphicsPipelineState(const GraphicsPipelineDesc& desc) { m_desc = desc; }
};

class VulkanRHIComputePipelineState final : public RHIComputePipelineState {
public:
    explicit VulkanRHIComputePipelineState(const ComputePipelineDesc& desc) { m_desc = desc; }
};

RHIRasterizerState* VulkanRHIPipelineStateFactory::CreateRasterizerState(
    const RasterizerDesc& desc) {
    LogStub("CreateRasterizerState");
    return new VulkanRHIRasterizerState(desc);
}
void VulkanRHIPipelineStateFactory::DestroyRasterizerState(
    RHIRasterizerState* state) { delete state; }

RHIBlendState* VulkanRHIPipelineStateFactory::CreateBlendState(
    const BlendDesc& desc) {
    LogStub("CreateBlendState");
    return new VulkanRHIBlendState(desc);
}
void VulkanRHIPipelineStateFactory::DestroyBlendState(RHIBlendState* state) {
    delete state;
}

RHIDepthStencilState* VulkanRHIPipelineStateFactory::CreateDepthStencilState(
    const DepthStencilDesc& desc) {
    LogStub("CreateDepthStencilState");
    return new VulkanRHIDepthStencilState(desc);
}
void VulkanRHIPipelineStateFactory::DestroyDepthStencilState(
    RHIDepthStencilState* state) { delete state; }

RHIInputLayout* VulkanRHIPipelineStateFactory::CreateInputLayout(
    const InputLayoutDesc& desc) {
    LogStub("CreateInputLayout");
    return new VulkanRHIInputLayout(desc);
}
void VulkanRHIPipelineStateFactory::DestroyInputLayout(RHIInputLayout* layout) {
    delete layout;
}

RHIGraphicsPipelineState* VulkanRHIPipelineStateFactory::CreateGraphicsPipelineState(
    const GraphicsPipelineDesc& desc) {
    LogStub("CreateGraphicsPipelineState");
    return new VulkanRHIGraphicsPipelineState(desc);
}
void VulkanRHIPipelineStateFactory::DestroyGraphicsPipelineState(
    RHIGraphicsPipelineState* pso) { delete pso; }

RHIComputePipelineState* VulkanRHIPipelineStateFactory::CreateComputePipelineState(
    const ComputePipelineDesc& desc) {
    LogStub("CreateComputePipelineState");
    return new VulkanRHIComputePipelineState(desc);
}
void VulkanRHIPipelineStateFactory::DestroyComputePipelineState(
    RHIComputePipelineState* pso) { delete pso; }

}  // namespace RHI
}  // namespace Luma
