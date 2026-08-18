#pragma once

#include <memory>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"
#include "Luma/RHI/RHITypes.h"

// RHI resource base classes and interfaces. Inspired by UE5's resource system
// but simplified for Luma's architecture. All GPU resources derive from RHIResource.

namespace Luma {
namespace RHI {

using namespace Luma::Math;

// Forward declarations
class RHIDevice;
class RHICommandList;

// ============================================================================
// Base Resource Class
// ============================================================================

// Base class for all RHI resources (buffers, textures, etc.)
class RHIResource {
public:
    virtual ~RHIResource() = default;
    
    // Get the resource type for RTTI
    virtual const char* GetResourceType() const = 0;
    
    // Get the size of the resource in bytes
    virtual u64 GetSize() const = 0;
    
    // Get the current resource state
    virtual EResourceState GetState() const = 0;
    
    // Set the resource state (used by command list transitions)
    virtual void SetState(EResourceState state) = 0;
    
    // Get debug name (if set)
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }
    
protected:
    std::string m_name;
};

// ============================================================================
// Buffer Resources
// ============================================================================

// Buffer creation description
struct BufferDesc {
    u64 size = 0;
    EBufferUsage usage = EBufferUsage::None;
    EBufferCPUAccess cpuAccess = EBufferCPUAccess::None;
    const void* initialData = nullptr;
    
    // For structured buffers
    u32 structureSize = 0;
    
    // Debug name
    std::string name;
};

// GPU buffer interface
class RHIBuffer : public RHIResource {
public:
    virtual ~RHIBuffer() = default;
    
    const char* GetResourceType() const override { return "Buffer"; }
    
    // Map the buffer for CPU access (if cpuAccess allows)
    virtual void* Map(u64 offset = 0, u64 size = 0) = 0;
    virtual void Unmap() = 0;
    
    // Update buffer data (for dynamic buffers)
    virtual void UpdateData(const void* data, u64 size, u64 offset = 0) = 0;
    
    // Get buffer description
    const BufferDesc& GetDesc() const { return m_desc; }
    
    // Get GPU address (for shader binding)
    virtual u64 GetGPUAddress() const = 0;
    
protected:
    BufferDesc m_desc;
};

// ============================================================================
// Texture Resources
// ============================================================================

// Texture creation description
struct TextureDesc {
    u32 width = 1;
    u32 height = 1;
    u32 depth = 1;
    u32 mipLevels = 1;
    u32 arraySize = 1;
    ESampleCount samples = ESampleCount::Count1;
    ETextureFormat format = ETextureFormat::R8G8B8A8_UNORM;
    ETextureUsage usage = ETextureUsage::None;
    ETextureFlags flags = ETextureFlags::None;
    const void* initialData = nullptr;
    
    // Debug name
    std::string name;
};

// GPU texture interface
class RHITexture : public RHIResource {
public:
    virtual ~RHITexture() = default;
    
    const char* GetResourceType() const override { return "Texture"; }
    
    // Get texture description
    const TextureDesc& GetDesc() const { return m_desc; }
    
    // Get subresource count
    u32 GetSubresourceCount() const;
    
    // Get texture dimension
    ETextureDimension GetDimension() const;
    
    // Check if texture is a cubemap
    bool IsCubemap() const { return (m_desc.flags & ETextureFlags::Cubemap) != ETextureFlags::None; }
    
    // Check if texture is an array
    bool IsArray() const { return (m_desc.flags & ETextureFlags::Array) != ETextureFlags::None; }
    
    // Check if texture is 3D
    bool IsVolume() const { return (m_desc.flags & ETextureFlags::Volume) != ETextureFlags::None; }
    
protected:
    TextureDesc m_desc;
};

// ============================================================================
// Resource Views
// ============================================================================

// Shader resource view (SRV) - for sampling textures/buffers in shaders
class RHIShaderResourceView {
public:
    virtual ~RHIShaderResourceView() = default;
    
    // Get the resource this view references
    virtual RHIResource* GetResource() const = 0;
    
    // Get view format
    virtual ETextureFormat GetFormat() const = 0;
};

// Unordered access view (UAV) - for read-write access in shaders
class RHIUnorderedAccessView {
public:
    virtual ~RHIUnorderedAccessView() = default;
    
    // Get the resource this view references
    virtual RHIResource* GetResource() const = 0;
    
    // Get view format
    virtual ETextureFormat GetFormat() const = 0;
};

// Render target view (RTV) - for rendering to textures
class RHIRenderTargetView {
public:
    virtual ~RHIRenderTargetView() = default;
    
    // Get the texture this view references
    virtual RHITexture* GetTexture() const = 0;
    
    // Get view format
    virtual ETextureFormat GetFormat() const = 0;
    
    // Get subresource range
    virtual const TextureSubresourceRange& GetSubresourceRange() const = 0;
};

// Depth-stencil view (DSV) - for depth-stencil rendering
class RHIDepthStencilView {
public:
    virtual ~RHIDepthStencilView() = default;
    
    // Get the texture this view references
    virtual RHITexture* GetTexture() const = 0;
    
    // Get view format
    virtual ETextureFormat GetFormat() const = 0;
    
    // Get subresource range
    virtual const TextureSubresourceRange& GetSubresourceRange() const = 0;
};

// ============================================================================
// Sampler State
// ============================================================================

// Texture filter mode
enum class ESamplerFilter : u32 {
    Point,
    Linear,
    Anisotropic,
};

// Texture address mode
enum class ESamplerAddressMode : u32 {
    Wrap,
    Mirror,
    Clamp,
    Border,
    MirrorOnce,
};

// Sampler comparison mode
enum class ESamplerComparisonFunc : u32 {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

// Sampler state description
struct SamplerDesc {
    ESamplerFilter filter = ESamplerFilter::Linear;
    ESamplerAddressMode addressU = ESamplerAddressMode::Wrap;
    ESamplerAddressMode addressV = ESamplerAddressMode::Wrap;
    ESamplerAddressMode addressW = ESamplerAddressMode::Wrap;
    f32 mipLodBias = 0.0f;
    f32 maxAnisotropy = 16.0f;
    ESamplerComparisonFunc comparisonFunc = ESamplerComparisonFunc::Never;
    f32 minLod = 0.0f;
    f32 maxLod = 1000.0f;
    Vec4 borderColor{0.0f, 0.0f, 0.0f, 0.0f};
    
    // Debug name
    std::string name;
};

// Sampler state interface
class RHISamplerState {
public:
    virtual ~RHISamplerState() = default;
    
    // Get sampler description
    const SamplerDesc& GetDesc() const { return m_desc; }
    
protected:
    SamplerDesc m_desc;
};

// ============================================================================
// Resource Creation Helpers
// ============================================================================

// Resource creation interface (implemented by RHI backends)
class RHIResourceFactory {
public:
    virtual ~RHIResourceFactory() = default;
    
    // Buffer creation
    virtual RHIBuffer* CreateBuffer(const BufferDesc& desc) = 0;
    
    // Texture creation
    virtual RHITexture* CreateTexture(const TextureDesc& desc) = 0;
    
    // View creation
    virtual RHIShaderResourceView* CreateShaderResourceView(RHIResource* resource, ETextureFormat format, const TextureSubresourceRange& range) = 0;
    virtual RHIUnorderedAccessView* CreateUnorderedAccessView(RHIResource* resource, ETextureFormat format, const TextureSubresourceRange& range) = 0;
    virtual RHIRenderTargetView* CreateRenderTargetView(RHITexture* texture, ETextureFormat format, const TextureSubresourceRange& range) = 0;
    virtual RHIDepthStencilView* CreateDepthStencilView(RHITexture* texture, ETextureFormat format, const TextureSubresourceRange& range) = 0;
    
    // Sampler creation
    virtual RHISamplerState* CreateSampler(const SamplerDesc& desc) = 0;
    
    // Resource destruction
    virtual void DestroyBuffer(RHIBuffer* buffer) = 0;
    virtual void DestroyTexture(RHITexture* texture) = 0;
    virtual void DestroyShaderResourceView(RHIShaderResourceView* view) = 0;
    virtual void DestroyUnorderedAccessView(RHIUnorderedAccessView* view) = 0;
    virtual void DestroyRenderTargetView(RHIRenderTargetView* view) = 0;
    virtual void DestroyDepthStencilView(RHIDepthStencilView* view) = 0;
    virtual void DestroySampler(RHISamplerState* sampler) = 0;
};

} // namespace RHI
} // namespace Luma