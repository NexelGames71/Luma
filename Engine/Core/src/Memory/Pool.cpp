#include "Luma/Core/Pool.h"

#include <cstring>

#include "Luma/Core/Assert.h"

namespace Luma::Memory {

Pool::Pool(usize block_size, usize block_count, Allocator& backing)
    : m_blockSize(block_size < sizeof(void*) ? sizeof(void*) : block_size),
      m_blockCount(block_count),
      m_backing(&backing) {
    LUMA_ASSERT(block_count > 0, "Pool must have at least one block");
    // The pool stores a free-list `next` pointer in the head of each free
    // block, so each block must be at least sizeof(void*) bytes.
    const usize bytes = m_blockSize * m_blockCount;
    m_buffer = backing.Alloc(bytes, kDefaultAlignment, AllocationSite{});
    LUMA_ASSERT(m_buffer != nullptr, "Pool backing allocation failed");

    // Build the singly-linked free list: each free block's first sizeof(void*)
    // bytes hold the pointer to the next free block; the last block holds null.
    byte* p = static_cast<byte*>(m_buffer);
    for (usize i = 0; i + 1 < m_blockCount; ++i) {
        void** next = reinterpret_cast<void**>(p + i * m_blockSize);
        *next = p + (i + 1) * m_blockSize;
    }
    void** last = reinterpret_cast<void**>(p + (m_blockCount - 1) * m_blockSize);
    *last = nullptr;
    m_freeList = p;
}

Pool::~Pool() {
    if (m_buffer && m_backing) {
        m_backing->Free(m_buffer, m_blockSize * m_blockCount, AllocationSite{});
    }
}

void* Pool::Alloc(usize size, usize align, const AllocationSite&) {
    LUMA_ASSERT(size <= m_blockSize, "Pool::Alloc size exceeds block size");
    LUMA_ASSERT(align <= kDefaultAlignment, "Pool only guarantees default alignment");
    if (!m_freeList) {
        LUMA_ASSERT(false, "Pool out of blocks");
        return nullptr;
    }
    void* p = m_freeList;
    m_freeList = *static_cast<void**>(p);
    ++m_allocatedCount;
    if (m_allocatedCount > m_peakAllocated) m_peakAllocated = m_allocatedCount;
    return p;
}

void* Pool::AllocZero(usize size, usize align, const AllocationSite& site) {
    void* p = Alloc(size, align, site);
    if (p) std::memset(p, 0, m_blockSize);  // zero the full block
    return p;
}

void Pool::Free(void* p, usize, const AllocationSite&) {
    if (!p) return;
    // Bounds check: must be a block we own.
    LUMA_ASSERT(p >= m_buffer &&
                static_cast<byte*>(p) <
                    static_cast<byte*>(m_buffer) + m_blockSize * m_blockCount,
                "Pool::Free of foreign pointer");
    void** next = static_cast<void**>(p);
    *next = m_freeList;
    m_freeList = p;
    --m_allocatedCount;
}

}  // namespace Luma::Memory
