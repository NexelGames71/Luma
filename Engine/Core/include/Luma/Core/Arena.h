#pragma once

#include <cstddef>

#include "Luma/Core/Alloc.h"
#include "Luma/Core/Types.h"

// Linear / arena allocator. One large backing buffer is carved out in order
// and reset all at once. Per-frame scratch allocators are Arenas.
//
// Trade-offs:
//   * Alloc is O(1) and very cheap (a bump pointer + alignment fixup).
//   * Individual frees are not supported — use Scope/Marker to bulk-free.
//   * Fragmentation is internal; once you Alloc, that slot is reserved until
//     the arena is reset or destroyed.
//
// A second concern: per-frame scratch is the highest-leverage use. Reset it
// at the END of every frame to keep it small; reset markers mid-frame to
// free everything since that marker (a stack-discipline scratch space).

namespace Luma::Memory {

class Arena : public Allocator {
public:
    // Takes ownership of `backing`; will free it on destruction. `backing` may
    // be null (empty arena; Alloc always fails).
    explicit Arena(void* backing, usize capacity);
    // Allocates its own buffer of `capacity` bytes from `backing_allocator`.
    // Pass `SystemHeap()` for an externally-owned arena.
    Arena(usize capacity, Allocator& backing_allocator);
    ~Arena() override;

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void* Alloc(usize size, usize align = kDefaultAlignment,
                const AllocationSite& site = {}) override;
    void* AllocZero(usize size, usize align = kDefaultAlignment,
                    const AllocationSite& site = {}) override;
    void Free(void* ptr, usize size = 0,
              const AllocationSite& site = {}) override;

    // Resets the bump pointer to the beginning. O(1). Does NOT call dtors.
    void Reset();

    // Returns the current "high water mark" — pass it back to RestoreTo to
    // rewind the arena to that point. Stack-disciplined scratch usage.
    usize Marker() const { return m_offset; }
    void RestoreTo(usize marker);

    // Stats.
    usize Capacity() const { return m_capacity; }
    usize Used() const { return m_offset; }
    usize PeakUsed() const { return m_peak; }
    usize AllocationCount() const { return m_count; }

private:
    void* m_backing = nullptr;
    usize m_capacity = 0;
    usize m_offset = 0;
    usize m_peak = 0;
    usize m_count = 0;
    bool m_owns_buffer = false;
    Allocator* m_backing_allocator = nullptr;
};

// RAII helper: restore the arena to its current marker on scope exit.
class ArenaScope {
public:
    explicit ArenaScope(Arena& a) : m_arena(&a), m_marker(a.Marker()) {}
    ~ArenaScope() { if (m_arena) m_arena->RestoreTo(m_marker); }
    ArenaScope(const ArenaScope&) = delete;
    ArenaScope& operator=(const ArenaScope&) = delete;
private:
    Arena* m_arena;
    usize m_marker;
};

// Returns the thread-local per-frame scratch arena. Each call returns the same
// arena; the framework is expected to call Reset() at the end of every frame
// so allocations don't accumulate.
Arena& FrameScratch();

// Resets the frame scratch. Call this once per frame, AFTER the work that
// needed scratch, so the next frame starts with a clean slate.
void ResetFrameScratch();

}  // namespace Luma::Memory
