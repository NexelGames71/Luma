#include "Luma/Core/Alloc.h"

#include <cstdlib>
#include <cstring>
#include <new>

#include "Luma/Core/Assert.h"

#if defined(_MSC_VER)
#include <malloc.h>  // _aligned_malloc / _aligned_free
#endif

namespace Luma::Memory {

// System heap — direct new/delete. Used for any backing buffer an Arena or
// Pool needs at construction time.
namespace {
class SystemHeapAllocator final : public Allocator {
public:
    void* Alloc(usize size, usize align, const AllocationSite&) override {
        if (size == 0) return nullptr;
        void* p = nullptr;
#if defined(_MSC_VER)
        p = _aligned_malloc(size, align);
#else
        // C11 aligned_alloc requires size to be a multiple of align; round up.
        usize rounded = (size + (align - 1)) & ~(align - 1);
        p = std::aligned_alloc(align, rounded);
#endif
        LUMA_ASSERT(p != nullptr, "SystemHeap out of memory");
        return p;
    }
    void* AllocZero(usize size, usize align, const AllocationSite& site) override {
        void* p = Alloc(size, align, site);
        if (p) std::memset(p, 0, size);
        return p;
    }
    void Free(void* p, usize, const AllocationSite&) override {
#if defined(_MSC_VER)
        _aligned_free(p);
#else
        std::free(p);
#endif
    }
};
SystemHeapAllocator g_systemHeap;
}  // namespace

Allocator& SystemHeap() { return g_systemHeap; }

// DebugHeap is just the system heap for now. The tracking layer is what
// gives the leak report at shutdown; DebugHeap exists as a separate
// accessor so a future allocator swap (e.g. mimalloc) is one line.
Allocator& DebugHeap() { return g_systemHeap; }

void* Alloc(Allocator& a, usize size, usize align, const AllocationSite& site) {
    return a.Alloc(size, align, site);
}
void* AllocZero(Allocator& a, usize size, usize align, const AllocationSite& site) {
    return a.AllocZero(size, align, site);
}
void Free(Allocator& a, void* p, usize size, const AllocationSite& site) {
    a.Free(p, size, site);
}

}  // namespace Luma::Memory
