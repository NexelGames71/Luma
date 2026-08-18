#include "Luma/UniformBuffer/UniformBuffer.h"
#include "Luma/RHI/RHIContext.h"

#include <cstring>
#include <algorithm>
#include <string>

namespace Luma {
namespace UniformBuffer {

using std::string;
using std::unique_ptr;
using std::vector;

// ============================================================================
// Uniform Buffer
// ============================================================================

UniformBuffer::UniformBuffer(const UniformBufferDesc& desc)
    : m_desc(desc) {
    // Create RHI buffer
    RHI::BufferDesc bufferDesc;
    bufferDesc.size = desc.size;
    bufferDesc.usage = RHI::EBufferUsage::Uniform;
    bufferDesc.cpuAccess = desc.cpuAccessible ? RHI::EBufferCPUAccess::Write : RHI::EBufferCPUAccess::None;
    bufferDesc.initialData = desc.initialData;
    
    // TODO: Get RHI context and create buffer
    // For now, this is a stub implementation
    m_rhiBuffer = nullptr;
}

UniformBuffer::~UniformBuffer() {
    if (m_mapped) {
        Unmap();
    }
    
    // TODO: Destroy RHI buffer
    if (m_rhiBuffer) {
        // GetRHIContext()->GetResourceFactory()->DestroyBuffer(m_rhiBuffer);
        m_rhiBuffer = nullptr;
    }
}

void* UniformBuffer::Map() {
    if (m_mapped) {
        return m_mappedData;
    }
    
    // TODO: Implement actual buffer mapping
    // For now, allocate temporary memory
    m_mappedData = new u8[m_desc.size];
    m_mapped = true;
    
    return m_mappedData;
}

void UniformBuffer::Unmap() {
    if (!m_mapped) {
        return;
    }
    
    // TODO: Flush data to GPU buffer before unmapping
    if (m_rhiBuffer && m_mappedData) {
        UpdateData(m_mappedData, m_desc.size);
    }
    
    delete[] static_cast<u8*>(m_mappedData);
    m_mappedData = nullptr;
    m_mapped = false;
}

void UniformBuffer::UpdateData(const void* data, u32 size, u32 offset) {
    if (!data || size == 0) {
        return;
    }
    
    if (offset + size > m_desc.size) {
        return;
    }
    
    if (m_mapped && m_mappedData) {
        // Update mapped memory
        std::memcpy(static_cast<u8*>(m_mappedData) + offset, data, size);
    } else {
        // TODO: Update GPU buffer directly
        // For now, this is a stub implementation
    }
}

void UniformBuffer::Update(const void* data) {
    UpdateData(data, m_desc.size, 0);
}

void UniformBuffer::Invalidate() {
    // TODO: Implement GPU cache invalidation
}

void UniformBuffer::Flush() {
    // TODO: Implement CPU cache flush
}

// ============================================================================
// Uniform Buffer Pool
// ============================================================================

UniformBufferPool::UniformBufferPool(u32 bufferSize, u32 bufferCount)
    : m_bufferSize(bufferSize), m_bufferCount(bufferCount) {
    
    m_buffers.reserve(bufferCount);
    for (u32 i = 0; i < bufferCount; ++i) {
        UniformBufferDesc desc;
        desc.size = bufferSize;
        desc.usage = EUniformBufferUsage::Dynamic;
        desc.cpuAccessible = true;
        desc.name = "PoolBuffer_" + std::to_string(i);
        
        m_buffers.push_back(unique_ptr<UniformBuffer>(new UniformBuffer(desc)));
    }
}

UniformBufferPool::~UniformBufferPool() {
    m_buffers.clear();
}

UniformBuffer* UniformBufferPool::GetBuffer(u32 index) {
    if (index >= m_buffers.size()) {
        return nullptr;
    }
    return m_buffers[index].get();
}

UniformBuffer* UniformBufferPool::GetNextBuffer() {
    if (m_buffers.empty()) {
        return nullptr;
    }
    
    UniformBuffer* buffer = m_buffers[m_currentIndex].get();
    m_currentIndex = (m_currentIndex + 1) % m_bufferCount;
    return buffer;
}

void UniformBufferPool::Reset() {
    m_currentIndex = 0;
}

void UniformBufferPool::Resize(u32 bufferCount) {
    if (bufferCount <= m_bufferCount) {
        return;
    }
    
    for (u32 i = m_bufferCount; i < bufferCount; ++i) {
        UniformBufferDesc desc;
        desc.size = m_bufferSize;
        desc.usage = EUniformBufferUsage::Dynamic;
        desc.cpuAccessible = true;
        desc.name = "PoolBuffer_" + std::to_string(i);
        
        m_buffers.push_back(unique_ptr<UniformBuffer>(new UniformBuffer(desc)));
    }
    
    m_bufferCount = bufferCount;
}

// ============================================================================
// Uniform Buffer Manager
// ============================================================================

UniformBufferManager& UniformBufferManager::GetInstance() {
    static UniformBufferManager instance;
    return instance;
}

UniformBufferManager::~UniformBufferManager() {
    Clear();
}

UniformBuffer* UniformBufferManager::CreateBuffer(const UniformBufferDesc& desc) {
    auto* buffer = new UniformBuffer(desc);
    m_buffers.push_back(buffer);
    return buffer;
}

void UniformBufferManager::DestroyBuffer(UniformBuffer* buffer) {
    auto it = std::find(m_buffers.begin(), m_buffers.end(), buffer);
    if (it != m_buffers.end()) {
        m_buffers.erase(it);
        delete buffer;
    }
}

UniformBufferPool* UniformBufferManager::CreatePool(u32 bufferSize, u32 bufferCount) {
    auto* pool = new UniformBufferPool(bufferSize, bufferCount);
    m_pools.push_back(pool);
    return pool;
}

void UniformBufferManager::DestroyPool(UniformBufferPool* pool) {
    auto it = std::find(m_pools.begin(), m_pools.end(), pool);
    if (it != m_pools.end()) {
        m_pools.erase(it);
        delete pool;
    }
}

UniformBuffer* UniformBufferManager::GetBuffer(const string& name) {
    for (auto* buffer : m_buffers) {
        if (buffer->GetName() == name) {
            return buffer;
        }
    }
    return nullptr;
}

void UniformBufferManager::Clear() {
    for (auto* buffer : m_buffers) {
        delete buffer;
    }
    m_buffers.clear();
    
    for (auto* pool : m_pools) {
        delete pool;
    }
    m_pools.clear();
}

} // namespace UniformBuffer
} // namespace Luma