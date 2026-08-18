#pragma once

#include <functional>
#include <memory>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"
#include "Luma/RHI/RHIResources.h"
#include "Luma/RHI/RHITypes.h"

// RHI command list interface for recording GPU commands. Inspired by UE5's
// command list pattern but simplified for Luma's architecture. Commands are
// recorded and then submitted to the GPU for execution.

namespace Luma {
namespace RHI {

using namespace Luma::Math;

// Forward declarations
class RHIDevice;
class RHIPipelineState;
class RHIBuffer;
class RHITexture;
class RHIShaderResourceView;
class RHIUnorderedAccessView;
class RHIRenderTargetView;
class RHIDepthStencilView;
class RHISamplerState;

// ============================================================================
// Command List Interface
// ============================================================================

// RHI command list for recording GPU commands
class RHICommandList {
public:
    virtual ~RHICommandList() = default;
    
    // Begin recording commands
    virtual void Begin() = 0;
    
    // End recording commands
    virtual void End() = 0;
    
    // Reset the command list for reuse
    virtual void Reset() = 0;
    
    // ============================================================================
    // Resource Barriers
    // ============================================================================
    
    // Transition a resource to a new state
    virtual void ResourceBarrier(RHIResource* resource, EResourceState newState) = 0;
    
    // Transition multiple resources
    virtual void ResourceBarrier(u32 count, RHIResource** resources, EResourceState* newStates) = 0;
    
    // Alias resource memory (for resource replacement)
    virtual void AliasingBarrier(RHIResource* before, RHIResource* after) = 0;
    
    // UAV barrier (for UAV synchronization)
    virtual void UAVBarrier(RHIResource* resource) = 0;
    
    // ============================================================================
    // Clear Operations
    // ============================================================================
    
    // Clear a render target view
    virtual void ClearRenderTargetView(RHIRenderTargetView* view, const Vec4& color) = 0;
    
    // Clear a depth-stencil view
    virtual void ClearDepthStencilView(RHIDepthStencilView* view, f32 depth, u8 stencil) = 0;
    
    // Clear a UAV (unordered access view)
    virtual void ClearUnorderedAccessView(RHIUnorderedAccessView* view, const Vec4& value) = 0;
    virtual void ClearUnorderedAccessView(RHIUnorderedAccessView* view, const u32 value[4]) = 0;
    
    // ============================================================================
    // Copy Operations
    // ============================================================================
    
    // Copy buffer to buffer
    virtual void CopyBuffer(RHIBuffer* src, RHIBuffer* dst, u64 srcOffset, u64 dstOffset, u64 size) = 0;
    
    // Copy texture to texture
    virtual void CopyTexture(RHITexture* src, RHITexture* dst, const TextureCopyRegion& region) = 0;
    
    // Copy buffer to texture
    virtual void CopyBufferToTexture(RHIBuffer* src, RHITexture* dst, const BufferToTextureCopy& region) = 0;
    
    // Copy texture to buffer
    virtual void CopyTextureToBuffer(RHITexture* src, RHIBuffer* dst, const BufferToTextureCopy& region) = 0;
    
    // Resolve multisampled texture
    virtual void ResolveTexture(RHITexture* src, RHITexture* dst) = 0;
    
    // ============================================================================
    // Rendering Commands
    // ============================================================================
    
    // Set render targets and depth-stencil
    virtual void SetRenderTargets(u32 numRTVs, RHIRenderTargetView** rtvs, RHIDepthStencilView* dsv) = 0;
    
    // Set viewport
    virtual void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth = 0.0f, f32 maxDepth = 1.0f) = 0;
    
    // Set scissor rect
    virtual void SetScissorRect(i32 x, i32 y, u32 width, u32 height) = 0;
    
    // Set pipeline state
    virtual void SetPipelineState(RHIPipelineState* pso) = 0;
    
    // Set vertex buffer
    virtual void SetVertexBuffer(u32 slot, RHIBuffer* buffer, u64 offset = 0) = 0;
    
    // Set index buffer
    virtual void SetIndexBuffer(RHIBuffer* buffer, u64 offset = 0) = 0;
    
    // Set shader resources
    virtual void SetShaderResources(u32 slot, RHIShaderResourceView* srv) = 0;
    virtual void SetShaderResources(u32 startSlot, u32 count, RHIShaderResourceView** srvs) = 0;
    
    // Set unordered access views
    virtual void SetUnorderedAccessViews(u32 slot, RHIUnorderedAccessView* uav) = 0;
    virtual void SetUnorderedAccessViews(u32 startSlot, u32 count, RHIUnorderedAccessView** uavs) = 0;
    
    // Set samplers
    virtual void SetSamplers(u32 slot, RHISamplerState* sampler) = 0;
    virtual void SetSamplers(u32 startSlot, u32 count, RHISamplerState** samplers) = 0;
    
    // Set primitive topology
    virtual void SetPrimitiveTopology(EPrimitiveTopology topology) = 0;
    
    // Draw commands
    virtual void Draw(u32 vertexCount, u32 startVertex = 0) = 0;
    virtual void DrawIndexed(u32 indexCount, u32 startIndex = 0, i32 baseVertex = 0) = 0;
    virtual void DrawInstanced(u32 vertexCount, u32 instanceCount, u32 startVertex = 0, u32 startInstance = 0) = 0;
    virtual void DrawIndexedInstanced(u32 indexCount, u32 instanceCount, u32 startIndex = 0, i32 baseVertex = 0, u32 startInstance = 0) = 0;
    
    // Indirect draw commands
    virtual void DrawIndirect(RHIBuffer* buffer, u64 offset) = 0;
    virtual void DrawIndexedIndirect(RHIBuffer* buffer, u64 offset) = 0;
    
    // ============================================================================
    // Compute Commands
    // ============================================================================
    
    // Set compute pipeline state
    virtual void SetComputePipelineState(RHIPipelineState* pso) = 0;
    
    // Dispatch compute shader
    virtual void Dispatch(u32 threadGroupCountX, u32 threadGroupCountY, u32 threadGroupCountZ) = 0;
    
    // Indirect dispatch
    virtual void DispatchIndirect(RHIBuffer* buffer, u64 offset) = 0;
    
    // ============================================================================
    // Query Operations
    // ============================================================================
    
    // Begin query
    virtual void BeginQuery(u32 queryIndex) = 0;
    
    // End query
    virtual void EndQuery(u32 queryIndex) = 0;
    
    // Resolve query (get results)
    virtual void ResolveQuery(u32 queryIndex) = 0;
    
    // ============================================================================
    // Debug and Markers
    // ============================================================================
    
    // Insert debug marker
    virtual void InsertDebugMarker(const char* name) = 0;
    
    // Begin debug region
    virtual void BeginDebugMarker(const char* name) = 0;
    
    // End debug region
    virtual void EndDebugMarker() = 0;
};

// ============================================================================
// Command List Builder (Helper for recording commands)
// ============================================================================

// Helper class for building command lists with RAII patterns
class RHICommandListBuilder {
public:
    explicit RHICommandListBuilder(RHICommandList* cmdList) : m_cmdList(cmdList) {
        m_cmdList->Begin();
    }
    
    ~RHICommandListBuilder() {
        m_cmdList->End();
    }
    
    RHICommandList* GetCommandList() const { return m_cmdList; }
    
    // RAII debug marker scope
    class DebugMarkerScope {
    public:
        DebugMarkerScope(RHICommandList* cmdList, const char* name) : m_cmdList(cmdList) {
            m_cmdList->BeginDebugMarker(name);
        }
        ~DebugMarkerScope() {
            m_cmdList->EndDebugMarker();
        }
    private:
        RHICommandList* m_cmdList;
    };
    
private:
    RHICommandList* m_cmdList;
};

// ============================================================================
// Command Queue
// ============================================================================

// Command queue for submitting command lists
class RHICommandQueue {
public:
    virtual ~RHICommandQueue() = default;
    
    // Submit command lists for execution
    virtual void Submit(u32 numLists, RHICommandList** lists) = 0;
    
    // Present the backbuffer
    virtual void Present() = 0;
    
    // Signal a fence
    virtual u64 Signal() = 0;
    
    // Wait for a fence
    virtual void Wait(u64 fenceValue) = 0;
    
    // Flush the queue (wait for all commands to complete)
    virtual void Flush() = 0;
    
    // Get the queue type
    virtual const char* GetQueueType() const = 0;
};

} // namespace RHI
} // namespace Luma