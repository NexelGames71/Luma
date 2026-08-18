#pragma once

#include "Luma/RHI/RHICommandList.h"

#include <vulkan/vulkan.h>

namespace Luma {
namespace RHI {

class VulkanRHIDevice;

// Vulkan-backed command list. Each instance owns a dedicated command pool +
// primary command buffer so it can be recorded, reset, and submitted
// independently. The pool is per-list (not shared) to avoid external
// synchronisation — cheap for the low command-list counts the deferred
// renderer needs.
class VulkanRHICommandList final : public RHICommandList {
public:
    /// Construct with a back-pointer to the device that created this list.
    /// Allocates a VkCommandPool + VkCommandBuffer internally.
    explicit VulkanRHICommandList(VulkanRHIDevice* device);
    ~VulkanRHICommandList() override;

    // ---- Accessors (backend-internal) ----------------------------------------
    VkCommandBuffer Handle() const { return m_cmd; }

    // Recording
    void Begin() override;
    void End() override;
    void Reset() override;

    // Barriers
    void ResourceBarrier(RHIResource* resource, EResourceState newState) override;
    void ResourceBarrier(u32 count, RHIResource** resources,
                         EResourceState* newStates) override;
    void AliasingBarrier(RHIResource* before, RHIResource* after) override;
    void UAVBarrier(RHIResource* resource) override;

    // Clear
    void ClearRenderTargetView(RHIRenderTargetView* view, const Vec4& color) override;
    void ClearDepthStencilView(RHIDepthStencilView* view, f32 depth, u8 stencil) override;
    void ClearUnorderedAccessView(RHIUnorderedAccessView* view, const Vec4& value) override;
    void ClearUnorderedAccessView(RHIUnorderedAccessView* view, const u32 value[4]) override;

    // Copy
    void CopyBuffer(RHIBuffer* src, RHIBuffer* dst, u64 srcOffset, u64 dstOffset, u64 size) override;
    void CopyTexture(RHITexture* src, RHITexture* dst,
                     const TextureCopyRegion& region) override;
    void CopyBufferToTexture(RHIBuffer* src, RHITexture* dst,
                             const BufferToTextureCopy& region) override;
    void CopyTextureToBuffer(RHITexture* src, RHIBuffer* dst,
                              const BufferToTextureCopy& region) override;
    void ResolveTexture(RHITexture* src, RHITexture* dst) override;

    // Render state
    void SetRenderTargets(u32 numRTVs, RHIRenderTargetView** rtvs,
                          RHIDepthStencilView* dsv) override;
    void SetViewport(f32 x, f32 y, f32 width, f32 height,
                     f32 minDepth, f32 maxDepth) override;
    void SetScissorRect(i32 x, i32 y, u32 width, u32 height) override;
    void SetPipelineState(RHIPipelineState* pso) override;
    void SetVertexBuffer(u32 slot, RHIBuffer* buffer, u64 offset) override;
    void SetIndexBuffer(RHIBuffer* buffer, u64 offset) override;
    void SetShaderResources(u32 slot, RHIShaderResourceView* srv) override;
    void SetShaderResources(u32 startSlot, u32 count,
                            RHIShaderResourceView** srvs) override;
    void SetUnorderedAccessViews(u32 slot, RHIUnorderedAccessView* uav) override;
    void SetUnorderedAccessViews(u32 startSlot, u32 count,
                                 RHIUnorderedAccessView** uavs) override;
    void SetSamplers(u32 slot, RHISamplerState* sampler) override;
    void SetSamplers(u32 startSlot, u32 count, RHISamplerState** samplers) override;
    void SetPrimitiveTopology(EPrimitiveTopology topology) override;

    // Draws
    void Draw(u32 vertexCount, u32 startVertex) override;
    void DrawIndexed(u32 indexCount, u32 startIndex, i32 baseVertex) override;
    void DrawInstanced(u32 vertexCount, u32 instanceCount, u32 startVertex,
                       u32 startInstance) override;
    void DrawIndexedInstanced(u32 indexCount, u32 instanceCount, u32 startIndex,
                              i32 baseVertex, u32 startInstance) override;
    void DrawIndirect(RHIBuffer* buffer, u64 offset) override;
    void DrawIndexedIndirect(RHIBuffer* buffer, u64 offset) override;

    // Compute
    void SetComputePipelineState(RHIPipelineState* pso) override;
    void Dispatch(u32 x, u32 y, u32 z) override;
    void DispatchIndirect(RHIBuffer* buffer, u64 offset) override;

    // Queries
    void BeginQuery(u32 queryIndex) override;
    void EndQuery(u32 queryIndex) override;
    void ResolveQuery(u32 queryIndex) override;

    // Debug
    void InsertDebugMarker(const char* name) override;
    void BeginDebugMarker(const char* name) override;
    void EndDebugMarker() override;

private:
    VulkanRHIDevice* m_device = nullptr;
    VkCommandPool    m_pool   = VK_NULL_HANDLE;
    VkCommandBuffer  m_cmd    = VK_NULL_HANDLE;
    bool             m_recording = false;

    // Track whether we are inside a dynamic rendering pass so EndRendering
    // can be issued before the next SetRenderTargets or End.
    bool             m_insideRenderPass = false;

    // Track current render pass dimensions for clear operations
    u32              m_renderPassWidth = 0;
    u32              m_renderPassHeight = 0;

    // Helpers
    void EndCurrentRenderPass();
};

}  // namespace RHI
}  // namespace Luma
