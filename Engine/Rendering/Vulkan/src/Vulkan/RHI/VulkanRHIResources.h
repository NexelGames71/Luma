#pragma once

#include <memory>
#include <vector>

#include "Luma/RHI/RHIResources.h"
#include "Vulkan/VulkanCommon.h"

// Vulkan-backed RHI resource implementations + the resource factory.
// These wrap raw Vulkan objects (VkBuffer/VkImage/VkImageView/VkSampler) so
// the deferred renderer can allocate GPU memory through the abstract RHI
// without seeing Vulkan types.
//
// Ownership: VulkanRHIBuffer / VulkanRHITexture own their Vulkan handles and
// destroy them in their destructors. Views are ref-counted peer objects that
// do not own the underlying resource; the factory owns view lifetimes.

namespace Luma {
namespace RHI {

class VulkanRHIDevice;

// ----------------------------------------------------------------------------
// VulkanRHIBuffer
// ----------------------------------------------------------------------------

class VulkanRHIBuffer final : public RHIBuffer {
public:
    // Constructs an empty buffer shell. The factory must call Init() before
    // the buffer is usable; this two-phase approach keeps the VkPhysicalDevice
    // out of the constructor signature (consistent with VulkanRHITexture).
    VulkanRHIBuffer(VkDevice device, const BufferDesc& desc);
    ~VulkanRHIBuffer() override;

    // Two-phase initialization: creates the VkBuffer + allocates + binds
    // memory (using `physical`). Returns true on success. On failure the
    // buffer is left unusable and should be destroyed by the caller.
    bool Init(VkPhysicalDevice physical);

    // RHIBuffer
    u64 GetSize() const override { return m_desc.size; }
    EResourceState GetState() const override { return m_state; }
    void SetState(EResourceState state) override { m_state = state; }
    void* Map(u64 offset = 0, u64 size = 0) override;
    void Unmap() override;
    void UpdateData(const void* data, u64 size, u64 offset = 0) override;
    u64 GetGPUAddress() const override;

    // Vulkan access (backend-internal)
    VkBuffer Handle() const { return m_buffer; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    void* m_mapped = nullptr;
    bool m_persistentMap = false;
    EResourceState m_state = EResourceState::Undefined;
};

// ----------------------------------------------------------------------------
// VulkanRHITexture
// ----------------------------------------------------------------------------

class VulkanRHITexture final : public RHITexture {
public:
    // Constructs an empty texture shell. The factory must call Init() before
    // the texture is usable; this two-phase approach keeps the VkPhysicalDevice
    // out of every method signature without exposing it publicly.
    VulkanRHITexture(VkDevice device, const TextureDesc& desc);
    ~VulkanRHITexture() override;

    // Two-phase initialization: creates the VkImage, allocates + binds memory
    // (using `physical`), and creates a default image view covering the full
    // resource. Returns true on success; on failure, the texture is left
    // unusable and should be destroyed by the caller.
    bool Init(VkPhysicalDevice physical);

    u64 GetSize() const override;
    EResourceState GetState() const override { return m_state; }
    void SetState(EResourceState state) override { m_state = state; }

    // Vulkan access (backend-internal)
    VkImage Handle() const { return m_image; }
    VkImageView DefaultView() const { return m_view; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    EResourceState m_state = EResourceState::Undefined;
};

// ----------------------------------------------------------------------------
// Views (lightweight; most methods return stored fields)
// ----------------------------------------------------------------------------

class VulkanRHIShaderResourceView final : public RHIShaderResourceView {
public:
    VulkanRHIShaderResourceView(RHIResource* resource, ETextureFormat format,
                                 TextureSubresourceRange range)
        : m_resource(resource), m_format(format), m_range(range) {}
    RHIResource* GetResource() const override { return m_resource; }
    ETextureFormat GetFormat() const override { return m_format; }
private:
    RHIResource* m_resource;
    ETextureFormat m_format;
    TextureSubresourceRange m_range;
};

class VulkanRHIUnorderedAccessView final : public RHIUnorderedAccessView {
public:
    VulkanRHIUnorderedAccessView(RHIResource* resource, ETextureFormat format,
                                  TextureSubresourceRange range)
        : m_resource(resource), m_format(format), m_range(range) {}
    RHIResource* GetResource() const override { return m_resource; }
    ETextureFormat GetFormat() const override { return m_format; }
private:
    RHIResource* m_resource;
    ETextureFormat m_format;
    TextureSubresourceRange m_range;
};

class VulkanRHIRenderTargetView final : public RHIRenderTargetView {
public:
    VulkanRHIRenderTargetView(RHITexture* texture, ETextureFormat format,
                                TextureSubresourceRange range)
        : m_texture(texture), m_format(format), m_range(range) {}
    RHITexture* GetTexture() const override { return m_texture; }
    ETextureFormat GetFormat() const override { return m_format; }
    const TextureSubresourceRange& GetSubresourceRange() const override { return m_range; }
private:
    RHITexture* m_texture;
    ETextureFormat m_format;
    TextureSubresourceRange m_range;
};

class VulkanRHIDepthStencilView final : public RHIDepthStencilView {
public:
    VulkanRHIDepthStencilView(RHITexture* texture, ETextureFormat format,
                                TextureSubresourceRange range)
        : m_texture(texture), m_format(format), m_range(range) {}
    RHITexture* GetTexture() const override { return m_texture; }
    ETextureFormat GetFormat() const override { return m_format; }
    const TextureSubresourceRange& GetSubresourceRange() const override { return m_range; }
private:
    RHITexture* m_texture;
    ETextureFormat m_format;
    TextureSubresourceRange m_range;
};

// ----------------------------------------------------------------------------
// VulkanRHISamplerState
// ----------------------------------------------------------------------------

class VulkanRHISamplerState final : public RHISamplerState {
public:
    explicit VulkanRHISamplerState(VkDevice device, const SamplerDesc& desc);
    ~VulkanRHISamplerState() override;

    VkSampler Handle() const { return m_sampler; }
private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};

// ----------------------------------------------------------------------------
// VulkanRHIResourceFactory
// ----------------------------------------------------------------------------

class VulkanRHIResourceFactory final : public RHIResourceFactory {
public:
    explicit VulkanRHIResourceFactory(VkPhysicalDevice physical, VkDevice device);
    ~VulkanRHIResourceFactory() override;

    // Buffer creation (implemented for real — the deferred renderer's GBuffer
    // path uses this when AllocateTarget is upgraded from the current stub).
    RHIBuffer* CreateBuffer(const BufferDesc& desc) override;

    // Texture creation (implemented for real — used by the GBuffer allocator
    // in step 7 of the staged adapter).
    RHITexture* CreateTexture(const TextureDesc& desc) override;

    // Views (lightweight; the Vulkan image view is created lazily by the
    // passes that need it; for now the view objects only carry metadata).
    RHIShaderResourceView* CreateShaderResourceView(
        RHIResource* resource, ETextureFormat format,
        const TextureSubresourceRange& range) override;
    RHIUnorderedAccessView* CreateUnorderedAccessView(
        RHIResource* resource, ETextureFormat format,
        const TextureSubresourceRange& range) override;
    RHIRenderTargetView* CreateRenderTargetView(
        RHITexture* texture, ETextureFormat format,
        const TextureSubresourceRange& range) override;
    RHIDepthStencilView* CreateDepthStencilView(
        RHITexture* texture, ETextureFormat format,
        const TextureSubresourceRange& range) override;

    RHISamplerState* CreateSampler(const SamplerDesc& desc) override;

    // Destruction
    void DestroyBuffer(RHIBuffer* buffer) override;
    void DestroyTexture(RHITexture* texture) override;
    void DestroyShaderResourceView(RHIShaderResourceView* view) override;
    void DestroyUnorderedAccessView(RHIUnorderedAccessView* view) override;
    void DestroyRenderTargetView(RHIRenderTargetView* view) override;
    void DestroyDepthStencilView(RHIDepthStencilView* view) override;
    void DestroySampler(RHISamplerState* sampler) override;

private:
    VkPhysicalDevice m_physical = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};

}  // namespace RHI
}  // namespace Luma
