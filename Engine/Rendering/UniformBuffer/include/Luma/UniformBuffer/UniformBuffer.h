#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/RHI/RHIResources.h"

// Uniform buffer system. Inspired by UE5's uniform buffer system but adapted
// for Luma's simpler architecture. Provides efficient GPU memory management
// for shader uniforms and constant data.

namespace Luma {
namespace UniformBuffer {

using std::string;
using std::vector;
using std::unique_ptr;

// ============================================================================
// Uniform Buffer Description
// ============================================================================

// Uniform buffer usage flags
enum class EUniformBufferUsage : u32 {
    Static,          // Updated rarely (once per frame or less)
    Dynamic,         // Updated frequently (multiple times per frame)
    Volatile,        // Updated very frequently (every draw call)
};

// Uniform buffer description
struct UniformBufferDesc {
    u32 size = 0;
    EUniformBufferUsage usage = EUniformBufferUsage::Static;
    bool cpuAccessible = true;
    const void* initialData = nullptr;
    string name;
};

// ============================================================================
// Uniform Buffer
// ============================================================================

// Uniform buffer class for managing GPU constant buffers
class UniformBuffer {
public:
    UniformBuffer(const UniformBufferDesc& desc);
    ~UniformBuffer();
    
    // Get buffer description
    const UniformBufferDesc& GetDesc() const { return m_desc; }
    
    // Get RHI buffer
    RHI::RHIBuffer* GetRHIBuffer() const { return m_rhiBuffer; }
    
    // Get buffer size
    u32 GetSize() const { return m_desc.size; }
    
    // Get buffer name
    const string& GetName() const { return m_desc.name; }
    
    // Map buffer for CPU access
    void* Map();
    
    // Unmap buffer
    void Unmap();
    
    // Update buffer data
    void UpdateData(const void* data, u32 size, u32 offset = 0);
    
    // Update entire buffer
    void Update(const void* data);
    
    // Check if buffer is mapped
    bool IsMapped() const { return m_mapped; }
    
    // Get mapped pointer
    void* GetMappedPointer() const { return m_mappedData; }
    
    // Invalidate GPU cache (for dynamic buffers)
    void Invalidate();
    
    // Flush CPU cache (for dynamic buffers)
    void Flush();
    
private:
    UniformBufferDesc m_desc;
    RHI::RHIBuffer* m_rhiBuffer = nullptr;
    void* m_mappedData = nullptr;
    bool m_mapped = false;
};

// ============================================================================
// Uniform Buffer Pool
// ============================================================================

// Pool for managing multiple uniform buffers efficiently
class UniformBufferPool {
public:
    UniformBufferPool(u32 bufferSize, u32 bufferCount);
    ~UniformBufferPool();
    
    // Get buffer size
    u32 GetBufferSize() const { return m_bufferSize; }
    
    // Get buffer count
    u32 GetBufferCount() const { return m_bufferCount; }
    
    // Get a buffer from the pool
    UniformBuffer* GetBuffer(u32 index);
    
    // Get next available buffer (round-robin)
    UniformBuffer* GetNextBuffer();
    
    // Reset pool
    void Reset();
    
    // Resize pool
    void Resize(u32 bufferCount);
    
private:
    u32 m_bufferSize;
    u32 m_bufferCount;
    u32 m_currentIndex = 0;
    vector<unique_ptr<UniformBuffer>> m_buffers;
};

// ============================================================================
// Uniform Buffer Manager
// ============================================================================

// Global manager for uniform buffer allocation and management
class UniformBufferManager {
public:
    static UniformBufferManager& GetInstance();
    
    // Create uniform buffer
    UniformBuffer* CreateBuffer(const UniformBufferDesc& desc);
    
    // Destroy uniform buffer
    void DestroyBuffer(UniformBuffer* buffer);
    
    // Create uniform buffer pool
    UniformBufferPool* CreatePool(u32 bufferSize, u32 bufferCount);
    
    // Destroy uniform buffer pool
    void DestroyPool(UniformBufferPool* pool);
    
    // Get buffer by name
    UniformBuffer* GetBuffer(const string& name);
    
    // Get all buffers
    const vector<UniformBuffer*>& GetAllBuffers() const { return m_buffers; }
    
    // Clear all buffers
    void Clear();
    
private:
    UniformBufferManager() = default;
    ~UniformBufferManager();
    
    vector<UniformBuffer*> m_buffers;
    vector<UniformBufferPool*> m_pools;
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Create uniform buffer with default settings
inline UniformBuffer* CreateUniformBuffer(u32 size, const void* data = nullptr) {
    UniformBufferDesc desc;
    desc.size = size;
    desc.usage = EUniformBufferUsage::Static;
    desc.cpuAccessible = true;
    desc.initialData = data;
    return UniformBufferManager::GetInstance().CreateBuffer(desc);
}

// Create dynamic uniform buffer
inline UniformBuffer* CreateDynamicUniformBuffer(u32 size) {
    UniformBufferDesc desc;
    desc.size = size;
    desc.usage = EUniformBufferUsage::Dynamic;
    desc.cpuAccessible = true;
    return UniformBufferManager::GetInstance().CreateBuffer(desc);
}

// Create volatile uniform buffer
inline UniformBuffer* CreateVolatileUniformBuffer(u32 size) {
    UniformBufferDesc desc;
    desc.size = size;
    desc.usage = EUniformBufferUsage::Volatile;
    desc.cpuAccessible = true;
    return UniformBufferManager::GetInstance().CreateBuffer(desc);
}

// Destroy uniform buffer
inline void DestroyUniformBuffer(UniformBuffer* buffer) {
    UniformBufferManager::GetInstance().DestroyBuffer(buffer);
}

} // namespace UniformBuffer
} // namespace Luma