#pragma once

#include <cstddef>

#include "Luma/Core/Types.h"

// Luma's allocator interface. Every concrete allocator implements these four
// methods. The default implementations throw (or LUMA_ASSERT in Debug); pure
// allocators (Arena, Pool) override the subset they need.
//
// Design intent:
//   * Trivially destructible / POD-friendly: Allocator has no virtual dtor and
//     no virtual functions; that lets us use it as a value member and keeps the
//     hot path non-virtual when the type is statically known.
//   * Source-aware: every allocation records file/line/function so the leak
//     report at shutdown can point at the offender.
//   * Composable: TrackingAllocator wraps any other Allocator to add tag+site
//     bookkeeping, so the rest of the engine doesn't need to special-case it.

namespace Luma::Memory {

// 16-byte aligned by default. Most SIMD / Vulkan structs require this.
inline constexpr usize kDefaultAlignment = 16;

struct AllocationSite {
    const char* file = nullptr;
    const char* function = nullptr;
    u32 line = 0;
    // Optional tag (string literal) used by the tracking layer to group
    // allocations under a label. Empty falls back to the function name.
    const char* tag = "";
};

// Plain Allocator: a source of raw bytes. Most code should use the helpers in
// the header below (Alloc/AllocZero/Free) rather than the methods directly.
class Allocator {
public:
    virtual ~Allocator() = default;

    // Returns nullptr on OOM (out of capacity for a bounded allocator).
    // `align` must be a power of two; the implementation is allowed to return a
    // pointer with stronger alignment than requested.
    virtual void* Alloc(usize size, usize align = kDefaultAlignment,
                        const AllocationSite& site = {}) = 0;
    virtual void* AllocZero(usize size, usize align = kDefaultAlignment,
                            const AllocationSite& site = {}) = 0;
    // Frees a pointer previously returned by Alloc/AllocZero on the same
    // allocator. Passing nullptr is a no-op. Free of a foreign pointer is
    // undefined behavior (the default implementation asserts).
    virtual void Free(void* ptr, usize size = 0,
                      const AllocationSite& site = {}) = 0;
};

// Alloc/Free helpers. Site is auto-filled by the LUMA_NEW/DELETE macros.
void* Alloc(Allocator& a, usize size, usize align = kDefaultAlignment,
            const AllocationSite& site = {});
void* AllocZero(Allocator& a, usize size, usize align = kDefaultAlignment,
                const AllocationSite& site = {});
void Free(Allocator& a, void* p, usize size = 0,
          const AllocationSite& site = {});

// Heap allocators — the two singletons the tracking layer wraps.
Allocator& SystemHeap();  // new/delete (or malloc/free)
Allocator& DebugHeap();   // new/delete + leak detection on shutdown

}  // namespace Luma::Memory
