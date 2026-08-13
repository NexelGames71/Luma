#include "Luma/Core/Arena.h"

#include <cstring>
#include <memory>
#include <new>

#include "Luma/Core/Assert.h"

namespace Luma::Memory {

namespace {

inline usize AlignUp(usize v, usize align) {
    return (v + (align - 1)) & ~(align - 1);
}

// Validation helper for power-of-two alignment.
inline bool IsPowerOfTwo(usize x) { return x != 0 && (x & (x - 1)) == 0; }

}  // namespace

// -- External buffer constructor ---------------------------------------------
Arena::Arena(void* backing, usize capacity)
    : m_backing(backing), m_capacity(capacity), m_owns_buffer(false) {}

// -- Self-allocated buffer constructor ---------------------------------------
Arena::Arena(usize capacity, Allocator& backing_allocator)
    : m_capacity(capacity), m_owns_buffer(true),
      m_backing_allocator(&backing_allocator) {
    if (capacity > 0) {
        m_backing = backing_allocator.Alloc(
            capacity, kDefaultAlignment, AllocationSite{});
        LUMA_ASSERT(m_backing != nullptr, "Arena backing buffer allocation failed");
    }
}

Arena::~Arena() {
    if (m_owns_buffer && m_backing && m_backing_allocator) {
        m_backing_allocator->Free(m_backing, m_capacity, AllocationSite{});
    }
}

void* Arena::Alloc(usize size, usize align, const AllocationSite&) {
    LUMA_ASSERT(IsPowerOfTwo(align), "Arena alignment must be a power of two");
    if (size == 0) return nullptr;
    usize aligned = AlignUp(m_offset, align);
    if (aligned + size > m_capacity) {
        LUMA_ASSERT(false, "Arena out of capacity");
        return nullptr;
    }
    void* p = static_cast<byte*>(m_backing) + aligned;
    m_offset = aligned + size;
    if (m_offset > m_peak) m_peak = m_offset;
    ++m_count;
    return p;
}

void* Arena::AllocZero(usize size, usize align, const AllocationSite& site) {
    void* p = Alloc(size, align, site);
    if (p) std::memset(p, 0, size);
    return p;
}

void Arena::Free(void*, usize, const AllocationSite&) {
    // No-op; arenas do not support individual frees. Use RestoreTo() or
    // Reset() instead. Reporting here would be too noisy; just accept the
    // idiom.
}

void Arena::Reset() {
    m_offset = 0;
    m_count = 0;
    // Peak is preserved on purpose — it's a "high water mark" over the
    // arena's lifetime, useful for sizing the next allocation.
}

void Arena::RestoreTo(usize marker) {
    LUMA_ASSERT(marker <= m_offset, "Arena::RestoreTo marker ahead of current offset");
    m_offset = marker;
    // Count is also restored to a "best effort": if we re-alloc into the
    // restored region the count will be incremented again. We don't try to
    // keep an exact site list here — arenas are scratch by design.
}

// -- Per-frame scratch -------------------------------------------------------
namespace {
constexpr usize kFrameScratchCapacity = 256 * 1024;  // 256 KB
std::unique_ptr<byte[]> g_frameScratchStorage;
std::unique_ptr<Arena> g_frameScratch;
}  // namespace

Arena& FrameScratch() {
    if (!g_frameScratch) {
        g_frameScratchStorage = std::unique_ptr<byte[]>(new byte[kFrameScratchCapacity]);
        g_frameScratch = std::make_unique<Arena>(
            g_frameScratchStorage.get(), kFrameScratchCapacity);
    }
    return *g_frameScratch;
}

void ResetFrameScratch() {
    if (g_frameScratch) g_frameScratch->Reset();
}

}  // namespace Luma::Memory
