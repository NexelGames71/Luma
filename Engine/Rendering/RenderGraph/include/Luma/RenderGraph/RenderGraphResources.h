#pragma once

#include "Luma/RenderGraph/RenderGraph.h"

// Render graph resources management. Provides utilities for creating and
// managing render graph resources like textures, buffers, and views.

namespace Luma {
namespace RenderGraph {

// Render graph resource factory for creating resources used in render graphs
class RenderGraphResourceFactory {
public:
    static RenderGraphResourceFactory& GetInstance();
    
    // Create a texture for render graph use
    RHI::RHITexture* CreateTexture(u32 width, u32 height, RHI::ETextureFormat format, RHI::ETextureUsage usage);
    
    // Create a buffer for render graph use
    RHI::RHIBuffer* CreateBuffer(u64 size, RHI::EBufferUsage usage);
    
    // Destroy a texture
    void DestroyTexture(RHI::RHITexture* texture);
    
    // Destroy a buffer
    void DestroyBuffer(RHI::RHIBuffer* buffer);
    
    // Clear all resources
    void Clear();
    
private:
    RenderGraphResourceFactory() = default;
    ~RenderGraphResourceFactory();
    
    vector<RHI::RHITexture*> m_textures;
    vector<RHI::RHIBuffer*> m_buffers;
};

// Render graph resource pool for reusing transient resources
class RenderGraphResourcePool {
public:
    RenderGraphResourcePool();
    ~RenderGraphResourcePool();
    
    // Get a texture from the pool (creates if not available)
    RHI::RHITexture* GetTexture(u32 width, u32 height, RHI::ETextureFormat format, RHI::ETextureUsage usage);
    
    // Get a buffer from the pool (creates if not available)
    RHI::RHIBuffer* GetBuffer(u64 size, RHI::EBufferUsage usage);
    
    // Return a texture to the pool
    void ReturnTexture(RHI::RHITexture* texture);
    
    // Return a buffer to the pool
    void ReturnBuffer(RHI::RHIBuffer* buffer);
    
    // Clear the pool
    void Clear();
    
    // Get pool statistics
    u32 GetTextureCount() const { return static_cast<u32>(m_texturePool.size()); }
    u32 GetBufferCount() const { return static_cast<u32>(m_bufferPool.size()); }
    
private:
    struct TextureKey {
        u32 width;
        u32 height;
        RHI::ETextureFormat format;
        RHI::ETextureUsage usage;
        
        bool operator==(const TextureKey& other) const {
            return width == other.width && height == other.height && 
                   format == other.format && usage == other.usage;
        }
    };
    
    struct TextureKeyHash {
        size_t operator()(const TextureKey& key) const {
            size_t h = ::std::hash<u32>()(key.width);
            h ^= ::std::hash<u32>()(key.height) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= ::std::hash<u32>()(static_cast<u32>(key.format)) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= ::std::hash<u32>()(static_cast<u32>(key.usage)) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    struct BufferKey {
        u64 size;
        RHI::EBufferUsage usage;
        
        bool operator==(const BufferKey& other) const {
            return size == other.size && usage == other.usage;
        }
    };
    
    struct BufferKeyHash {
        size_t operator()(const BufferKey& key) const {
            size_t h = ::std::hash<u64>()(key.size);
            h ^= ::std::hash<u32>()(static_cast<u32>(key.usage)) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    unordered_map<TextureKey, vector<RHI::RHITexture*>, TextureKeyHash> m_texturePool;
    unordered_map<BufferKey, vector<RHI::RHIBuffer*>, BufferKeyHash> m_bufferPool;
};

// Convenience functions for creating render graph resources
inline RHI::RHITexture* CreateRenderTexture(u32 width, u32 height, RHI::ETextureFormat format) {
    return RenderGraphResourceFactory::GetInstance().CreateTexture(width, height, format, RHI::ETextureUsage::ShaderResource | RHI::ETextureUsage::RenderTarget);
}

inline RHI::RHITexture* CreateDepthTexture(u32 width, u32 height, RHI::ETextureFormat format = RHI::ETextureFormat::D24_UNORM_S8_UINT) {
    return RenderGraphResourceFactory::GetInstance().CreateTexture(width, height, format, RHI::ETextureUsage::DepthStencil);
}

inline RHI::RHIBuffer* CreateRenderBuffer(u64 size) {
    return RenderGraphResourceFactory::GetInstance().CreateBuffer(size, RHI::EBufferUsage::Uniform);
}

} // namespace RenderGraph
} // namespace Luma