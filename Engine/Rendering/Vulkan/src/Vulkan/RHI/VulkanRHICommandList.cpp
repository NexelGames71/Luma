#include "Vulkan/RHI/VulkanRHICommandList.h"
#include "Vulkan/RHI/VulkanRHIDevice.h"
#include "Vulkan/RHI/VulkanRHIResources.h"
#include "Vulkan/VulkanDevice.h"

#include "Luma/Core/Log.h"

namespace Luma {
namespace RHI {

// ============================================================================
// Helpers
// ============================================================================

namespace {

VkFormat ToVkFormat(ETextureFormat format);  // defined in VulkanRHIResources.cpp

// Re-declare the format helper locally — the one in VulkanRHIResources.cpp
// lives in an anonymous namespace and isn't linkable. For the handful of
// formats the deferred renderer actually uses, this small table is enough.
VkFormat FormatToVk(ETextureFormat format) {
    switch (format) {
        case ETextureFormat::R8G8B8A8_UNORM:        return VK_FORMAT_R8G8B8A8_UNORM;
        case ETextureFormat::R8G8B8A8_SRGB:         return VK_FORMAT_R8G8B8A8_SRGB;
        case ETextureFormat::R16G16B16A16_FLOAT:     return VK_FORMAT_R16G16B16A16_SFLOAT;
        case ETextureFormat::R32G32B32A32_FLOAT:     return VK_FORMAT_R32G32B32A32_SFLOAT;
        case ETextureFormat::R16G16_FLOAT:           return VK_FORMAT_R16G16_SFLOAT;
        case ETextureFormat::R32_FLOAT:              return VK_FORMAT_R32_SFLOAT;
        case ETextureFormat::D16_UNORM:              return VK_FORMAT_D16_UNORM;
        case ETextureFormat::D24_UNORM_S8_UINT:      return VK_FORMAT_D24_UNORM_S8_UINT;
        case ETextureFormat::D32_FLOAT:              return VK_FORMAT_D32_SFLOAT;
        case ETextureFormat::D32_FLOAT_S8_UINT:      return VK_FORMAT_D32_SFLOAT_S8_UINT;
        default:                                      return VK_FORMAT_UNDEFINED;
    }
}

bool IsDepthFormat(ETextureFormat fmt) {
    return fmt == ETextureFormat::D16_UNORM ||
           fmt == ETextureFormat::D24_UNORM_S8_UINT ||
           fmt == ETextureFormat::D32_FLOAT ||
           fmt == ETextureFormat::D32_FLOAT_S8_UINT;
}

void LogStub(const char* method) {
    LUMA_LOG_WARN("RHI", "VulkanRHICommandList: {} not yet implemented", method);
}

}  // namespace

// ============================================================================
// Construction / destruction
// ============================================================================

VulkanRHICommandList::VulkanRHICommandList(VulkanRHIDevice* device)
    : m_device(device) {
    VkDevice vkDevice = device->LogicalHandle();

    // Create a dedicated command pool for this list (resettable).
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = *device->Backend()->Queues().graphics;
    if (vkCreateCommandPool(vkDevice, &poolInfo, nullptr, &m_pool) != VK_SUCCESS) {
        LUMA_LOG_ERROR("RHI", "VulkanRHICommandList: failed to create command pool");
        return;
    }

    // Allocate one primary command buffer.
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(vkDevice, &allocInfo, &m_cmd) != VK_SUCCESS) {
        LUMA_LOG_ERROR("RHI", "VulkanRHICommandList: failed to allocate command buffer");
        m_cmd = VK_NULL_HANDLE;
    }
}

VulkanRHICommandList::~VulkanRHICommandList() {
    if (m_pool != VK_NULL_HANDLE && m_device) {
        // Free command buffer before destroying pool
        if (m_cmd != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(m_device->LogicalHandle(), m_pool, 1, &m_cmd);
            m_cmd = VK_NULL_HANDLE;
        }
        // Destroying the pool implicitly frees all command buffers from it.
        vkDestroyCommandPool(m_device->LogicalHandle(), m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
}

// ============================================================================
// Recording lifecycle
// ============================================================================

void VulkanRHICommandList::Begin() {
    if (!m_cmd) return;
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(m_cmd, &beginInfo);
    m_recording = true;
}

void VulkanRHICommandList::End() {
    if (!m_cmd || !m_recording) return;
    EndCurrentRenderPass();
    vkEndCommandBuffer(m_cmd);
    m_recording = false;
}

void VulkanRHICommandList::Reset() {
    if (!m_cmd) return;
    m_insideRenderPass = false;
    m_recording = false;
    vkResetCommandBuffer(m_cmd, 0);
}

// ============================================================================
// Barriers
// ============================================================================

void VulkanRHICommandList::ResourceBarrier(RHIResource* resource,
                                           EResourceState newState) {
    if (!m_cmd || !resource) return;

    // Try to cast to a texture — the deferred renderer's barriers are almost
    // exclusively image layout transitions.
    auto* tex = dynamic_cast<VulkanRHITexture*>(resource);
    if (!tex) {
        // Buffer barriers are less common; log and skip for now.
        LogStub("ResourceBarrier(buffer)");
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = tex->Handle();
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    // Determine aspect from the texture format.
    ETextureFormat fmt = tex->GetDesc().format;
    if (IsDepthFormat(fmt)) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (fmt == ETextureFormat::D24_UNORM_S8_UINT ||
            fmt == ETextureFormat::D32_FLOAT_S8_UINT)
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    // Map old state → layout + access.
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    EResourceState oldState = tex->GetState();

    auto mapState = [](EResourceState s, VkImageLayout& layout,
                       VkAccessFlags& access, VkPipelineStageFlags& stage, bool& isUndefined) {
        switch (s) {
            case EResourceState::Undefined:
            case EResourceState::Common:
                layout = VK_IMAGE_LAYOUT_UNDEFINED;
                access = 0;
                stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                isUndefined = true;
                break;
            case EResourceState::RenderTarget:
                layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                stage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                isUndefined = false;
                break;
            case EResourceState::DepthWrite:
                layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                stage  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                isUndefined = false;
                break;
            case EResourceState::DepthRead:
                layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                stage  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                isUndefined = false;
                break;
            case EResourceState::ShaderResource:
                layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                access = VK_ACCESS_SHADER_READ_BIT;
                stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                isUndefined = false;
                break;
            case EResourceState::UnorderedAccess:
                layout = VK_IMAGE_LAYOUT_GENERAL;
                access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                isUndefined = false;
                break;
            case EResourceState::Present:
                layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                access = 0;
                stage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                isUndefined = false;
                break;
            default:
                layout = VK_IMAGE_LAYOUT_GENERAL;
                access = 0;
                stage  = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                isUndefined = false;
                break;
        }
    };

    bool oldUndefined = false, newUndefined = false;
    mapState(oldState, barrier.oldLayout, barrier.srcAccessMask, srcStage, oldUndefined);
    mapState(newState, barrier.newLayout, barrier.dstAccessMask, dstStage, newUndefined);
    
    // If transitioning from undefined, skip the barrier (Vulkan spec says this is valid)
    if (oldUndefined) {
        tex->SetState(newState);
        return;
    }

    vkCmdPipelineBarrier(m_cmd, srcStage, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);
    tex->SetState(newState);
}

void VulkanRHICommandList::ResourceBarrier(u32 count, RHIResource** resources,
                                           EResourceState* newStates) {
    for (u32 i = 0; i < count; ++i)
        ResourceBarrier(resources[i], newStates[i]);
}

void VulkanRHICommandList::AliasingBarrier(RHIResource* /*before*/,
                                           RHIResource* /*after*/) {
    LogStub("AliasingBarrier");
}

void VulkanRHICommandList::UAVBarrier(RHIResource* /*resource*/) {
    LogStub("UAVBarrier");
}

// ============================================================================
// Clear operations
// ============================================================================

void VulkanRHICommandList::ClearRenderTargetView(RHIRenderTargetView* /*view*/,
                                                 const Vec4& color) {
    if (!m_cmd || !m_insideRenderPass) return;
    VkClearAttachment clear{};
    clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear.colorAttachment = 0;
    clear.clearValue.color = {{color.x, color.y, color.z, color.w}};
    VkClearRect rect{};
    rect.layerCount = 1;
    rect.rect.extent = {m_renderPassWidth, m_renderPassHeight};
    vkCmdClearAttachments(m_cmd, 1, &clear, 1, &rect);
}

void VulkanRHICommandList::ClearDepthStencilView(RHIDepthStencilView* /*view*/,
                                                 f32 depth, u8 stencil) {
    if (!m_cmd || !m_insideRenderPass) return;
    VkClearAttachment clear{};
    clear.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    clear.clearValue.depthStencil = {depth, stencil};
    VkClearRect rect{};
    rect.layerCount = 1;
    rect.rect.extent = {m_renderPassWidth, m_renderPassHeight};
    vkCmdClearAttachments(m_cmd, 1, &clear, 1, &rect);
}

void VulkanRHICommandList::ClearUnorderedAccessView(
    RHIUnorderedAccessView* /*view*/, const Vec4& /*value*/) {
    LogStub("ClearUnorderedAccessView(float)");
}
void VulkanRHICommandList::ClearUnorderedAccessView(
    RHIUnorderedAccessView* /*view*/, const u32 /*value*/[4]) {
    LogStub("ClearUnorderedAccessView(uint)");
}

// ============================================================================
// Copy operations (stubs — not needed for the initial deferred pipeline)
// ============================================================================

void VulkanRHICommandList::CopyBuffer(RHIBuffer*, RHIBuffer*, u64, u64, u64) { LogStub("CopyBuffer"); }
void VulkanRHICommandList::CopyTexture(RHITexture*, RHITexture*, const TextureCopyRegion&) { LogStub("CopyTexture"); }
void VulkanRHICommandList::CopyBufferToTexture(RHIBuffer*, RHITexture*, const BufferToTextureCopy&) { LogStub("CopyBufferToTexture"); }
void VulkanRHICommandList::CopyTextureToBuffer(RHITexture*, RHIBuffer*, const BufferToTextureCopy&) { LogStub("CopyTextureToBuffer"); }
void VulkanRHICommandList::ResolveTexture(RHITexture*, RHITexture*) { LogStub("ResolveTexture"); }

// ============================================================================
// Render state — dynamic rendering (Vulkan 1.3)
// ============================================================================

void VulkanRHICommandList::EndCurrentRenderPass() {
    if (m_insideRenderPass && m_cmd) {
        vkCmdEndRendering(m_cmd);
        m_insideRenderPass = false;
    }
}

void VulkanRHICommandList::SetRenderTargets(u32 numRTVs,
                                            RHIRenderTargetView** rtvs,
                                            RHIDepthStencilView* dsv) {
    if (!m_cmd) return;
    EndCurrentRenderPass();

    // Build color attachments from the RTV array.
    VkRenderingAttachmentInfo colorAttachments[8]{};
    u32 width = 0, height = 0;
    for (u32 i = 0; i < numRTVs && i < 8; ++i) {
        auto* rtv = rtvs[i];
        if (!rtv || !rtv->GetTexture()) continue;
        auto* vkTex = static_cast<VulkanRHITexture*>(rtv->GetTexture());
        colorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachments[i].imageView = vkTex->DefaultView();
        colorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        if (width == 0) {
            width  = vkTex->GetDesc().width;
            height = vkTex->GetDesc().height;
        }
    }

    // Depth attachment.
    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    bool hasDepth = false;
    if (dsv && dsv->GetTexture()) {
        auto* vkTex = static_cast<VulkanRHITexture*>(dsv->GetTexture());
        depthAttachment.imageView = vkTex->DefaultView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        hasDepth = true;
        if (width == 0) {
            width  = vkTex->GetDesc().width;
            height = vkTex->GetDesc().height;
        }
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = {width, height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = numRTVs;
    renderingInfo.pColorAttachments = numRTVs > 0 ? colorAttachments : nullptr;
    renderingInfo.pDepthAttachment = hasDepth ? &depthAttachment : nullptr;
    vkCmdBeginRendering(m_cmd, &renderingInfo);
    m_insideRenderPass = true;
    
    // Store render pass dimensions for clear operations
    m_renderPassWidth = width;
    m_renderPassHeight = height;
}

void VulkanRHICommandList::SetViewport(f32 x, f32 y, f32 width, f32 height,
                                       f32 minDepth, f32 maxDepth) {
    if (!m_cmd) return;
    VkViewport vp{x, y, width, height, minDepth, maxDepth};
    vkCmdSetViewport(m_cmd, 0, 1, &vp);
}

void VulkanRHICommandList::SetScissorRect(i32 x, i32 y, u32 width, u32 height) {
    if (!m_cmd) return;
    VkRect2D scissor{{x, y}, {width, height}};
    vkCmdSetScissor(m_cmd, 0, 1, &scissor);
}

void VulkanRHICommandList::SetPipelineState(RHIPipelineState* /*pso*/) {
    // Pipeline state objects don't wrap a real VkPipeline yet — the deferred
    // renderer will create and bind its own VkPipelines directly through the
    // DeferredShadingRenderer until the PSO factory is fully wired.
    LogStub("SetPipelineState");
}

void VulkanRHICommandList::SetVertexBuffer(u32 /*slot*/, RHIBuffer* buffer,
                                           u64 offset) {
    if (!m_cmd || !buffer) return;
    auto* vkBuf = static_cast<VulkanRHIBuffer*>(buffer);
    VkBuffer handle = vkBuf->Handle();
    VkDeviceSize off = static_cast<VkDeviceSize>(offset);
    vkCmdBindVertexBuffers(m_cmd, 0, 1, &handle, &off);
}

void VulkanRHICommandList::SetIndexBuffer(RHIBuffer* buffer, u64 offset) {
    if (!m_cmd || !buffer) return;
    auto* vkBuf = static_cast<VulkanRHIBuffer*>(buffer);
    vkCmdBindIndexBuffer(m_cmd, vkBuf->Handle(),
                         static_cast<VkDeviceSize>(offset),
                         VK_INDEX_TYPE_UINT32);
}

void VulkanRHICommandList::SetShaderResources(u32, RHIShaderResourceView*) {
    LogStub("SetShaderResources(single)");
}
void VulkanRHICommandList::SetShaderResources(u32, u32, RHIShaderResourceView**) {
    LogStub("SetShaderResources(multi)");
}
void VulkanRHICommandList::SetUnorderedAccessViews(u32, RHIUnorderedAccessView*) {
    LogStub("SetUnorderedAccessViews(single)");
}
void VulkanRHICommandList::SetUnorderedAccessViews(u32, u32, RHIUnorderedAccessView**) {
    LogStub("SetUnorderedAccessViews(multi)");
}
void VulkanRHICommandList::SetSamplers(u32, RHISamplerState*) {
    LogStub("SetSamplers(single)");
}
void VulkanRHICommandList::SetSamplers(u32, u32, RHISamplerState**) {
    LogStub("SetSamplers(multi)");
}

void VulkanRHICommandList::SetPrimitiveTopology(EPrimitiveTopology /*topology*/) {
    // Vulkan 1.3 dynamic state — can be wired once the PSO factory bakes
    // VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY into pipelines.
    LogStub("SetPrimitiveTopology");
}

// ============================================================================
// Draw commands
// ============================================================================

void VulkanRHICommandList::Draw(u32 vertexCount, u32 startVertex) {
    if (!m_cmd) return;
    vkCmdDraw(m_cmd, vertexCount, 1, startVertex, 0);
}

void VulkanRHICommandList::DrawIndexed(u32 indexCount, u32 startIndex,
                                       i32 baseVertex) {
    if (!m_cmd) return;
    vkCmdDrawIndexed(m_cmd, indexCount, 1, startIndex, baseVertex, 0);
}

void VulkanRHICommandList::DrawInstanced(u32 vertexCount, u32 instanceCount,
                                         u32 startVertex, u32 startInstance) {
    if (!m_cmd) return;
    vkCmdDraw(m_cmd, vertexCount, instanceCount, startVertex, startInstance);
}

void VulkanRHICommandList::DrawIndexedInstanced(u32 indexCount,
                                                u32 instanceCount,
                                                u32 startIndex,
                                                i32 baseVertex,
                                                u32 startInstance) {
    if (!m_cmd) return;
    vkCmdDrawIndexed(m_cmd, indexCount, instanceCount, startIndex,
                     baseVertex, startInstance);
}

void VulkanRHICommandList::DrawIndirect(RHIBuffer* /*buffer*/, u64 /*offset*/) {
    LogStub("DrawIndirect");
}
void VulkanRHICommandList::DrawIndexedIndirect(RHIBuffer* /*buffer*/, u64 /*offset*/) {
    LogStub("DrawIndexedIndirect");
}

// ============================================================================
// Compute (stubs — not needed for the initial deferred pipeline)
// ============================================================================

void VulkanRHICommandList::SetComputePipelineState(RHIPipelineState*) { LogStub("SetComputePipelineState"); }
void VulkanRHICommandList::Dispatch(u32, u32, u32) { LogStub("Dispatch"); }
void VulkanRHICommandList::DispatchIndirect(RHIBuffer*, u64) { LogStub("DispatchIndirect"); }

// ============================================================================
// Queries (stubs)
// ============================================================================

void VulkanRHICommandList::BeginQuery(u32) { LogStub("BeginQuery"); }
void VulkanRHICommandList::EndQuery(u32) { LogStub("EndQuery"); }
void VulkanRHICommandList::ResolveQuery(u32) { LogStub("ResolveQuery"); }

// ============================================================================
// Debug markers
// ============================================================================

void VulkanRHICommandList::InsertDebugMarker(const char* /*name*/) {}
void VulkanRHICommandList::BeginDebugMarker(const char* /*name*/) {}
void VulkanRHICommandList::EndDebugMarker() {}

}  // namespace RHI
}  // namespace Luma
