#include "Luma/RenderGraph/RenderGraphResources.h"
#include "Luma/RHI/RHIContext.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Luma {
namespace RenderGraph {

using std::string;
using std::vector;

// ============================================================================
// Render Graph Resource Factory
// ============================================================================

RenderGraphResourceFactory& RenderGraphResourceFactory::GetInstance() {
    static RenderGraphResourceFactory instance;
    return instance;
}

RenderGraphResourceFactory::~RenderGraphResourceFactory() {
    Clear();
}

RHI::RHITexture* RenderGraphResourceFactory::CreateTexture(u32 width, u32 height, RHI::ETextureFormat format, RHI::ETextureUsage usage) {
    RHI::TextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.format = format;
    desc.usage = usage;
    
    // TODO: Get RHI context and create texture
    // For now, return nullptr as stub
    return nullptr;
}

RHI::RHIBuffer* RenderGraphResourceFactory::CreateBuffer(u64 size, RHI::EBufferUsage usage) {
    RHI::BufferDesc desc;
    desc.size = size;
    desc.usage = usage;
    desc.cpuAccess = RHI::EBufferCPUAccess::None;
    
    // TODO: Get RHI context and create buffer
    // For now, return nullptr as stub
    return nullptr;
}

void RenderGraphResourceFactory::DestroyTexture(RHI::RHITexture* texture) {
    if (!texture) {
        return;
    }
    
    auto it = std::find(m_textures.begin(), m_textures.end(), texture);
    if (it != m_textures.end()) {
        // TODO: Get RHI context and destroy texture
        m_textures.erase(it);
    }
}

void RenderGraphResourceFactory::DestroyBuffer(RHI::RHIBuffer* buffer) {
    if (!buffer) {
        return;
    }
    
    auto it = std::find(m_buffers.begin(), m_buffers.end(), buffer);
    if (it != m_buffers.end()) {
        // TODO: Get RHI context and destroy buffer
        m_buffers.erase(it);
    }
}

void RenderGraphResourceFactory::Clear() {
    // Clear all tracking vectors (actual resource destruction will be handled by RHI context)
    m_textures.clear();
    m_buffers.clear();
}

// ============================================================================
// Render Graph Resource Pool
// ============================================================================

RenderGraphResourcePool::RenderGraphResourcePool() {
}

RenderGraphResourcePool::~RenderGraphResourcePool() {
    Clear();
}

RHI::RHITexture* RenderGraphResourcePool::GetTexture(u32 width, u32 height, RHI::ETextureFormat format, RHI::ETextureUsage usage) {
    TextureKey key;
    key.width = width;
    key.height = height;
    key.format = format;
    key.usage = usage;
    
    // Check if we have a matching texture in the pool
    auto it = m_texturePool.find(key);
    if (it != m_texturePool.end() && !it->second.empty()) {
        auto* texture = it->second.back();
        it->second.pop_back();
        return texture;
    }
    
    // Create a new texture using the factory
    return RenderGraphResourceFactory::GetInstance().CreateTexture(width, height, format, usage);
}

RHI::RHIBuffer* RenderGraphResourcePool::GetBuffer(u64 size, RHI::EBufferUsage usage) {
    BufferKey key;
    key.size = size;
    key.usage = usage;
    
    // Check if we have a matching buffer in the pool
    auto it = m_bufferPool.find(key);
    if (it != m_bufferPool.end() && !it->second.empty()) {
        auto* buffer = it->second.back();
        it->second.pop_back();
        return buffer;
    }
    
    // Create a new buffer using the factory
    return RenderGraphResourceFactory::GetInstance().CreateBuffer(size, usage);
}

void RenderGraphResourcePool::ReturnTexture(RHI::RHITexture* texture) {
    if (!texture) {
        return;
    }
    
    // TODO: Get texture properties and return to appropriate pool
    // For now, just return to factory for destruction
    RenderGraphResourceFactory::GetInstance().DestroyTexture(texture);
}

void RenderGraphResourcePool::ReturnBuffer(RHI::RHIBuffer* buffer) {
    if (!buffer) {
        return;
    }
    
    // TODO: Get buffer properties and return to appropriate pool
    // For now, just return to factory for destruction
    RenderGraphResourceFactory::GetInstance().DestroyBuffer(buffer);
}

void RenderGraphResourcePool::Clear() {
    // Return all pooled resources to factory for destruction
    for (auto& [key, textures] : m_texturePool) {
        for (auto* texture : textures) {
            RenderGraphResourceFactory::GetInstance().DestroyTexture(texture);
        }
        textures.clear();
    }
    m_texturePool.clear();
    
    for (auto& [key, buffers] : m_bufferPool) {
        for (auto* buffer : buffers) {
            RenderGraphResourceFactory::GetInstance().DestroyBuffer(buffer);
        }
        buffers.clear();
    }
    m_bufferPool.clear();
}

} // namespace RenderGraph
} // namespace Luma