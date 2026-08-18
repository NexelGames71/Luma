#include "Vulkan/RHI/VulkanRHIResources.h"

#include <cstring>

#include "Luma/Core/Assert.h"
#include "Luma/Core/Log.h"
#include "Vulkan/VulkanMemory.h"

namespace Luma {
namespace RHI {

namespace {

// ---- Format translation -------------------------------------------------

VkFormat ToVkFormat(ETextureFormat format) {
    switch (format) {
        case ETextureFormat::R8_UNORM: return VK_FORMAT_R8_UNORM;
        case ETextureFormat::R8_SNORM: return VK_FORMAT_R8_SNORM;
        case ETextureFormat::R8_UINT: return VK_FORMAT_R8_UINT;
        case ETextureFormat::R8_SINT: return VK_FORMAT_R8_SINT;
        case ETextureFormat::R16_UNORM: return VK_FORMAT_R16_UNORM;
        case ETextureFormat::R16_SNORM: return VK_FORMAT_R16_SNORM;
        case ETextureFormat::R16_UINT: return VK_FORMAT_R16_UINT;
        case ETextureFormat::R16_SINT: return VK_FORMAT_R16_SINT;
        case ETextureFormat::R16_FLOAT: return VK_FORMAT_R16_SFLOAT;
        case ETextureFormat::R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
        case ETextureFormat::R8G8_SNORM: return VK_FORMAT_R8G8_SNORM;
        case ETextureFormat::R8G8_UINT: return VK_FORMAT_R8G8_UINT;
        case ETextureFormat::R8G8_SINT: return VK_FORMAT_R8G8_SINT;
        case ETextureFormat::R16G16_UNORM: return VK_FORMAT_R16G16_UNORM;
        case ETextureFormat::R16G16_SNORM: return VK_FORMAT_R16G16_SNORM;
        case ETextureFormat::R16G16_UINT: return VK_FORMAT_R16G16_UINT;
        case ETextureFormat::R16G16_SINT: return VK_FORMAT_R16G16_SINT;
        case ETextureFormat::R16G16_FLOAT: return VK_FORMAT_R16G16_SFLOAT;
        case ETextureFormat::R32_UINT: return VK_FORMAT_R32_UINT;
        case ETextureFormat::R32_SINT: return VK_FORMAT_R32_SINT;
        case ETextureFormat::R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
        case ETextureFormat::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case ETextureFormat::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case ETextureFormat::R8G8B8A8_UINT: return VK_FORMAT_R8G8B8A8_UINT;
        case ETextureFormat::R8G8B8A8_SINT: return VK_FORMAT_R8G8B8A8_SINT;
        case ETextureFormat::R16G16B16A16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM;
        case ETextureFormat::R16G16B16A16_SNORM: return VK_FORMAT_R16G16B16A16_SNORM;
        case ETextureFormat::R16G16B16A16_UINT: return VK_FORMAT_R16G16B16A16_UINT;
        case ETextureFormat::R16G16B16A16_SINT: return VK_FORMAT_R16G16B16A16_SINT;
        case ETextureFormat::R16G16B16A16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case ETextureFormat::R32G32_UINT: return VK_FORMAT_R32G32_UINT;
        case ETextureFormat::R32G32_SINT: return VK_FORMAT_R32G32_SINT;
        case ETextureFormat::R32G32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case ETextureFormat::R32G32B32A32_UINT: return VK_FORMAT_R32G32B32A32_UINT;
        case ETextureFormat::R32G32B32A32_SINT: return VK_FORMAT_R32G32B32A32_SINT;
        case ETextureFormat::R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case ETextureFormat::D16_UNORM: return VK_FORMAT_D16_UNORM;
        case ETextureFormat::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
        case ETextureFormat::D32_FLOAT: return VK_FORMAT_D32_SFLOAT;
        case ETextureFormat::D32_FLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        default: return VK_FORMAT_UNDEFINED;
    }
}

VkImageAspectFlags AspectForFormat(VkFormat format) {
    if (format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT)
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    if (format == VK_FORMAT_D24_UNORM_S8_UINT ||
        format == VK_FORMAT_D32_SFLOAT_S8_UINT)
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

// For shader sampling, depth-stencil textures should use only depth aspect
VkImageAspectFlags AspectForFormatShaderSampling(VkFormat format) {
    if (format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT ||
        format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT)
        return VK_IMAGE_ASPECT_DEPTH_BIT;  // Only depth for shader sampling
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageUsageFlags ToVkImageUsage(ETextureUsage usage, ETextureFlags flags) {
    VkImageUsageFlags vk = 0;
    if ((usage & ETextureUsage::ShaderResource) != ETextureUsage::None)
        vk |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if ((usage & ETextureUsage::RenderTarget) != ETextureUsage::None)
        vk |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if ((usage & ETextureUsage::DepthStencil) != ETextureUsage::None)
        vk |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if ((usage & ETextureUsage::UnorderedAccess) != ETextureUsage::None)
        vk |= VK_IMAGE_USAGE_STORAGE_BIT;
    if ((usage & ETextureUsage::TransferSrc) != ETextureUsage::None)
        vk |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if ((usage & ETextureUsage::TransferDst) != ETextureUsage::None)
        vk |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if ((flags & ETextureFlags::RenderTargetable) != ETextureFlags::None)
        vk |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if ((flags & ETextureFlags::DepthStencilTargetable) != ETextureFlags::None)
        vk |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if ((flags & ETextureFlags::ShaderResource) != ETextureFlags::None)
        vk |= VK_IMAGE_USAGE_SAMPLED_BIT;
    vk |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return vk;
}

VkBufferUsageFlags ToVkBufferUsage(EBufferUsage usage) {
    VkBufferUsageFlags flags = 0;
    if ((usage & EBufferUsage::Vertex) != EBufferUsage::None)
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if ((usage & EBufferUsage::Index) != EBufferUsage::None)
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if ((usage & EBufferUsage::Uniform) != EBufferUsage::None)
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if ((usage & EBufferUsage::Structured) != EBufferUsage::None)
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if ((usage & EBufferUsage::TransferSrc) != EBufferUsage::None)
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if ((usage & EBufferUsage::TransferDst) != EBufferUsage::None)
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return flags;
}

VkMemoryPropertyFlags ToVkMemoryProperties(EBufferCPUAccess cpu) {
    if (cpu == EBufferCPUAccess::None)
        return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
}

}  // namespace

// ============================================================================
// VulkanRHIBuffer
// ============================================================================

VulkanRHIBuffer::VulkanRHIBuffer(VkDevice device, const BufferDesc& desc)
    : m_device(device) {
    m_desc = desc;
    // EBufferCPUAccess has no operator& defined; cast to u32 for the bit test.
    m_persistentMap =
        (static_cast<u32>(desc.cpuAccess) &
         static_cast<u32>(EBufferCPUAccess::Write)) != 0 ||
        (static_cast<u32>(desc.cpuAccess) &
         static_cast<u32>(EBufferCPUAccess::Read)) != 0;
    // Allocation happens in Init(); the constructor only stashes the desc.
}

VulkanRHIBuffer::~VulkanRHIBuffer() {
    if (m_device == VK_NULL_HANDLE) return;
    GpuBuffer gpu{m_buffer, m_memory, m_desc.size, m_persistentMap ? m_mapped : nullptr};
    DestroyBuffer(m_device, gpu);
    m_mapped = nullptr;
}

bool VulkanRHIBuffer::Init(VkPhysicalDevice physical) {
    const VkBufferUsageFlags usage = ToVkBufferUsage(m_desc.usage);
    const VkMemoryPropertyFlags memProps =
        ToVkMemoryProperties(m_desc.cpuAccess);
    // EBufferCPUAccess has no operator& defined; cast to u32 for the bit test.
    const bool cpuReadable =
        (static_cast<u32>(m_desc.cpuAccess) &
         static_cast<u32>(EBufferCPUAccess::Read)) != 0;
    const bool cpuWritable =
        (static_cast<u32>(m_desc.cpuAccess) &
         static_cast<u32>(EBufferCPUAccess::Write)) != 0;
    (void)cpuReadable; (void)cpuWritable;

    GpuBuffer gpu = CreateBuffer(physical, m_device, m_desc.size, usage,
                                 memProps, m_persistentMap);
    m_buffer = gpu.buffer;
    m_memory = gpu.memory;
    m_mapped = gpu.mapped;

    if (m_buffer == VK_NULL_HANDLE) {
        LUMA_LOG_ERROR("RHI", "VulkanRHIBuffer::Init failed (size={})",
                       m_desc.size);
        return false;
    }

    if (m_desc.initialData && m_mapped) {
        std::memcpy(m_mapped, m_desc.initialData,
                    static_cast<usize>(m_desc.size));
    }

    m_state = (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                  ? EResourceState::ConstantBuffer
                  : ((usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
                         ? EResourceState::VertexBuffer
                         : EResourceState::Common);
    return true;
}

void* VulkanRHIBuffer::Map(u64 /*offset*/, u64 /*size*/) {
    if (m_mapped) return m_mapped;
    if (m_device == VK_NULL_HANDLE || m_memory == VK_NULL_HANDLE) return nullptr;
    void* ptr = nullptr;
    VkResult r = vkMapMemory(m_device, m_memory, 0, VK_WHOLE_SIZE, 0, &ptr);
    if (r != VK_SUCCESS) {
        LUMA_LOG_ERROR("RHI", "vkMapMemory failed: {}", VkResultString(r));
        return nullptr;
    }
    m_mapped = ptr;
    return m_mapped;
}

void VulkanRHIBuffer::Unmap() {
    if (m_device == VK_NULL_HANDLE || !m_mapped) return;
    if (!m_persistentMap) {
        vkUnmapMemory(m_device, m_memory);
        m_mapped = nullptr;
    }
}

void VulkanRHIBuffer::UpdateData(const void* data, u64 size, u64 offset) {
    if (m_device == VK_NULL_HANDLE || m_memory == VK_NULL_HANDLE) return;
    if (size == 0 || !data) return;
    void* mapped = Map(offset, size);
    if (!mapped) return;
    std::memcpy(static_cast<u8*>(mapped) + offset, data, size);
    Unmap();
}

u64 VulkanRHIBuffer::GetGPUAddress() const { return 0; }

// ============================================================================
// VulkanRHITexture
// ============================================================================

VulkanRHITexture::VulkanRHITexture(VkDevice device, const TextureDesc& desc)
    : m_device(device) {
    m_desc = desc;
    // The image is created in Init(); the constructor only stashes the desc.
}

VulkanRHITexture::~VulkanRHITexture() {
    if (m_device == VK_NULL_HANDLE) return;
    if (m_view) vkDestroyImageView(m_device, m_view, nullptr);
    if (m_memory) vkFreeMemory(m_device, m_memory, nullptr);
    if (m_image) vkDestroyImage(m_device, m_image, nullptr);
}

bool VulkanRHITexture::Init(VkPhysicalDevice physical) {
    const VkFormat format = ToVkFormat(m_desc.format);
    if (format == VK_FORMAT_UNDEFINED) {
        LUMA_LOG_ERROR("RHI", "VulkanRHITexture: unsupported format {}",
                       static_cast<u32>(m_desc.format));
        return false;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType =
        (m_desc.flags & ETextureFlags::Volume) != ETextureFlags::None
            ? VK_IMAGE_TYPE_3D
            : VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {m_desc.width, m_desc.height, 1};
    imageInfo.mipLevels = m_desc.mipLevels;
    imageInfo.arrayLayers = m_desc.arraySize;
    imageInfo.samples = static_cast<VkSampleCountFlagBits>(m_desc.samples);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = ToVkImageUsage(m_desc.usage, m_desc.flags);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
        LUMA_LOG_ERROR("RHI", "vkCreateImage failed ({}x{})", m_desc.width,
                       m_desc.height);
        m_image = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryRequirements reqs{};
    vkGetImageMemoryRequirements(m_device, m_image, &reqs);

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physical, &memProps);

    constexpr VkMemoryPropertyFlags kPreferred =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    u32 chosen = ~0u;
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool typeMatches = (reqs.memoryTypeBits & (1u << i)) != 0;
        const bool propsMatch =
            (memProps.memoryTypes[i].propertyFlags & kPreferred) == kPreferred;
        if (typeMatches && propsMatch) { chosen = i; break; }
    }
    if (chosen == ~0u) {
        for (u32 i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((reqs.memoryTypeBits & (1u << i)) != 0) { chosen = i; break; }
        }
    }
    if (chosen == ~0u) {
        LUMA_LOG_ERROR("RHI", "no memory type matched for texture");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = reqs.size;
    allocInfo.memoryTypeIndex = chosen;
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        LUMA_LOG_ERROR("RHI", "vkAllocateMemory failed for texture");
        m_memory = VK_NULL_HANDLE;
        return false;
    }
    vkBindImageMemory(m_device, m_image, m_memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = (m_desc.arraySize > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                                : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    // Use shader-sampling aspect mask for textures that will be sampled in shaders
    if ((m_desc.usage & ETextureUsage::ShaderResource) != ETextureUsage::None) {
        viewInfo.subresourceRange.aspectMask = AspectForFormatShaderSampling(format);
    } else {
        viewInfo.subresourceRange.aspectMask = AspectForFormat(format);
    }
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = m_desc.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = m_desc.arraySize;
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_view) != VK_SUCCESS) {
        LUMA_LOG_ERROR("RHI", "vkCreateImageView failed for texture");
        m_view = VK_NULL_HANDLE;
        return false;
    }

    m_state = (m_desc.usage & ETextureUsage::RenderTarget) != ETextureUsage::None
                   ? EResourceState::RenderTarget
                   : ((m_desc.usage & ETextureUsage::DepthStencil) !=
                          ETextureUsage::None
                          ? EResourceState::DepthWrite
                          : EResourceState::ShaderResource);
    return true;
}

u64 VulkanRHITexture::GetSize() const {
    return u64(m_desc.width) * m_desc.height * m_desc.arraySize *
           GetFormatSize(m_desc.format);
}

// ============================================================================
// VulkanRHISamplerState
// ============================================================================

namespace {

VkFilter ToVkFilter(ESamplerFilter f) {
    switch (f) {
        case ESamplerFilter::Point: return VK_FILTER_NEAREST;
        case ESamplerFilter::Linear:
        case ESamplerFilter::Anisotropic: return VK_FILTER_LINEAR;
    }
    return VK_FILTER_LINEAR;
}

VkSamplerAddressMode ToVkAddressMode(ESamplerAddressMode m) {
    switch (m) {
        case ESamplerAddressMode::Wrap: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case ESamplerAddressMode::Mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case ESamplerAddressMode::Clamp: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case ESamplerAddressMode::Border: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case ESamplerAddressMode::MirrorOnce: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkCompareOp ToVkCompareOp(ESamplerComparisonFunc f) {
    switch (f) {
        case ESamplerComparisonFunc::Never: return VK_COMPARE_OP_NEVER;
        case ESamplerComparisonFunc::Less: return VK_COMPARE_OP_LESS;
        case ESamplerComparisonFunc::Equal: return VK_COMPARE_OP_EQUAL;
        case ESamplerComparisonFunc::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case ESamplerComparisonFunc::Greater: return VK_COMPARE_OP_GREATER;
        case ESamplerComparisonFunc::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
        case ESamplerComparisonFunc::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case ESamplerComparisonFunc::Always: return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_NEVER;
}

}  // namespace

VulkanRHISamplerState::VulkanRHISamplerState(VkDevice device,
                                                const SamplerDesc& desc) {
    m_device = device;
    m_desc = desc;

    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = ToVkFilter(desc.filter);
    info.minFilter = ToVkFilter(desc.filter);
    info.mipmapMode = (desc.filter == ESamplerFilter::Anisotropic)
                          ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                          : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU = ToVkAddressMode(desc.addressU);
    info.addressModeV = ToVkAddressMode(desc.addressV);
    info.addressModeW = ToVkAddressMode(desc.addressW);
    info.mipLodBias = desc.mipLodBias;
    info.anisotropyEnable =
        desc.filter == ESamplerFilter::Anisotropic ? VK_TRUE : VK_FALSE;
    info.maxAnisotropy = desc.maxAnisotropy;
    info.compareEnable =
        desc.comparisonFunc != ESamplerComparisonFunc::Never ? VK_TRUE : VK_FALSE;
    info.compareOp = ToVkCompareOp(desc.comparisonFunc);
    info.minLod = desc.minLod;
    info.maxLod = desc.maxLod;
    info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

    if (vkCreateSampler(device, &info, nullptr, &m_sampler) != VK_SUCCESS) {
        LUMA_LOG_ERROR("RHI", "vkCreateSampler failed");
        m_sampler = VK_NULL_HANDLE;
    }
}

VulkanRHISamplerState::~VulkanRHISamplerState() {
    if (m_device != VK_NULL_HANDLE && m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
    }
}

// ============================================================================
// VulkanRHIResourceFactory
// ============================================================================

VulkanRHIResourceFactory::VulkanRHIResourceFactory(VkPhysicalDevice physical,
                                                       VkDevice device)
    : m_physical(physical), m_device(device) {}

VulkanRHIResourceFactory::~VulkanRHIResourceFactory() = default;

RHIBuffer* VulkanRHIResourceFactory::CreateBuffer(const BufferDesc& desc) {
    auto* buf = new VulkanRHIBuffer(m_device, desc);
    if (!buf->Init(m_physical)) {
        delete buf;
        return nullptr;
    }
    return buf;
}

RHITexture* VulkanRHIResourceFactory::CreateTexture(const TextureDesc& desc) {
    auto* tex = new VulkanRHITexture(m_device, desc);
    if (!tex->Init(m_physical)) {
        delete tex;
        return nullptr;
    }
    return tex;
}

RHIShaderResourceView* VulkanRHIResourceFactory::CreateShaderResourceView(
    RHIResource* resource, ETextureFormat format,
    const TextureSubresourceRange& range) {
    return new VulkanRHIShaderResourceView(resource, format, range);
}

RHIUnorderedAccessView* VulkanRHIResourceFactory::CreateUnorderedAccessView(
    RHIResource* /*resource*/, ETextureFormat format,
    const TextureSubresourceRange& range) {
    return new VulkanRHIUnorderedAccessView(nullptr, format, range);
}

RHIRenderTargetView* VulkanRHIResourceFactory::CreateRenderTargetView(
    RHITexture* texture, ETextureFormat format,
    const TextureSubresourceRange& range) {
    return new VulkanRHIRenderTargetView(texture, format, range);
}

RHIDepthStencilView* VulkanRHIResourceFactory::CreateDepthStencilView(
    RHITexture* texture, ETextureFormat format,
    const TextureSubresourceRange& range) {
    return new VulkanRHIDepthStencilView(texture, format, range);
}

RHISamplerState* VulkanRHIResourceFactory::CreateSampler(const SamplerDesc& desc) {
    return new VulkanRHISamplerState(m_device, desc);
}

void VulkanRHIResourceFactory::DestroyBuffer(RHIBuffer* buffer) { delete buffer; }
void VulkanRHIResourceFactory::DestroyTexture(RHITexture* texture) { delete texture; }
void VulkanRHIResourceFactory::DestroyShaderResourceView(RHIShaderResourceView* view) { delete view; }
void VulkanRHIResourceFactory::DestroyUnorderedAccessView(RHIUnorderedAccessView* view) { delete view; }
void VulkanRHIResourceFactory::DestroyRenderTargetView(RHIRenderTargetView* view) { delete view; }
void VulkanRHIResourceFactory::DestroyDepthStencilView(RHIDepthStencilView* view) { delete view; }
void VulkanRHIResourceFactory::DestroySampler(RHISamplerState* sampler) { delete sampler; }

}  // namespace RHI
}  // namespace Luma
