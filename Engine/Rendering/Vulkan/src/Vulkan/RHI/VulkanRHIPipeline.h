#pragma once

#include "Luma/RHI/RHIPipeline.h"

namespace Luma {
namespace RHI {

class VulkanRHIDevice;

// Stub pipeline state factory. Each Create returns a state object holding
// the input desc (so callers can introspect it in subsequent passes);
// destruction deletes the state. No Vulkan handle is created yet — the
// deferred renderer's first stage never reaches PipelineState creation.
// First-use emits a warning so the next stage knows where to fill in.
class VulkanRHIPipelineStateFactory final : public RHIPipelineStateFactory {
public:
    VulkanRHIPipelineStateFactory() = default;
    ~VulkanRHIPipelineStateFactory() override = default;

    RHIRasterizerState* CreateRasterizerState(const RasterizerDesc& desc) override;
    void DestroyRasterizerState(RHIRasterizerState* state) override;

    RHIBlendState* CreateBlendState(const BlendDesc& desc) override;
    void DestroyBlendState(RHIBlendState* state) override;

    RHIDepthStencilState* CreateDepthStencilState(const DepthStencilDesc& desc) override;
    void DestroyDepthStencilState(RHIDepthStencilState* state) override;

    RHIInputLayout* CreateInputLayout(const InputLayoutDesc& desc) override;
    void DestroyInputLayout(RHIInputLayout* layout) override;

    RHIGraphicsPipelineState* CreateGraphicsPipelineState(
        const GraphicsPipelineDesc& desc) override;
    void DestroyGraphicsPipelineState(RHIGraphicsPipelineState* pso) override;

    RHIComputePipelineState* CreateComputePipelineState(
        const ComputePipelineDesc& desc) override;
    void DestroyComputePipelineState(RHIComputePipelineState* pso) override;
};

}  // namespace RHI
}  // namespace Luma
