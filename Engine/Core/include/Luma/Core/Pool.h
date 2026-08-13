#pragma once

#include <cstddef>

#include "Luma/Core/Alloc.h"
#include "Luma/Core/Types.h"

// Fixed-size block pool. All allocations have the same size (set at
// construction); Alloc returns a free block in O(1) and Free pushes it back
// onto the free list. Backed by an Arena, so the entire pool is destroyed at
// once with no per-block dealloc cost.
//
// Use it for: ECS components of the same type, transform nodes, small
// frequently-allocated structs. Don't use it for variable-size data.

namespace Luma::Memory {

class Pool : public Allocator {
public:
    // `block_size` is the user-visible payload size. The pool will add
    // alignment padding + an internal free-list next pointer as needed.
    // `block_count` is the number of blocks preallocated.
    Pool(usize block_size, usize block_count, Allocator& backing);
    ~Pool() override;

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    void* Alloc(usize size, usize align = kDefaultAlignment,
                const AllocationSite& site = {}) override;
    void* AllocZero(usize size, usize align = kDefaultAlignment,
                    const AllocationSite& site = {}) override;
    void Free(void* ptr, usize size = 0,
              const AllocationSite& site = {}) override;

    // Stats.
    usize Capacity() const { return m_blockCount; }
    usize Used() const { return m_allocatedCount; }
    usize PeakUsed() const { return m_peakAllocated; }
    usize BlockSize() const { return m_blockSize; }

private:
    void* m_buffer = nullptr;
    void* m_freeList = nullptr;
    usize m_blockSize = 0;
    usize m_blockCount = 0;
    usize m_allocatedCount = 0;
    usize m_peakAllocated = 0;
    Allocator* m_backing = nullptr;
};

}  // namespace Luma::Memory
