#pragma once

#include <cstddef>
#include <string_view>

#include "Luma/Core/Alloc.h"
#include "Luma/Core/Types.h"

// Per-allocation tag and site tracking. Wraps any Allocator to add
// (string-literal tag, source location, size) bookkeeping. The Tracking layer
// is what powers the shutdown leak report and the peak/current counters.
//
// The wrapper forwards every Alloc/Free to the wrapped allocator; the bookkeeping
// runs alongside. Tracking is the default in Debug + Development builds; the
// TrackingAllocator is a no-op pass-through in Shipping (or when the user
// explicitly requests "release" semantics).
//
// Shipped out of the box:
//   * Tag list with current/peak byte counters
//   * Per-allocation site list (capped, to bound memory growth)
//   * At shutdown, ReportLeaks() dumps outstanding allocations sorted by size.

namespace Luma::Memory {

struct TagStats {
    std::string_view tag;
    usize currentBytes = 0;
    usize peakBytes = 0;
    usize currentCount = 0;
    usize peakCount = 0;
    usize totalAllocs = 0;
    usize totalFrees = 0;
};

struct SiteInfo {
    void* ptr = nullptr;
    usize size = 0;
    const char* tag = "";
    const char* file = nullptr;
    const char* function = nullptr;
    u32 line = 0;
};

class TrackingAllocator : public Allocator {
public:
    explicit TrackingAllocator(Allocator& inner) : m_inner(&inner) {}
    ~TrackingAllocator() override;

    TrackingAllocator(const TrackingAllocator&) = delete;
    TrackingAllocator& operator=(const TrackingAllocator&) = delete;

    void* Alloc(usize size, usize align = kDefaultAlignment,
                const AllocationSite& site = {}) override;
    void* AllocZero(usize size, usize align = kDefaultAlignment,
                    const AllocationSite& site = {}) override;
    void Free(void* ptr, usize size = 0,
              const AllocationSite& site = {}) override;

    // Snapshot of current tag stats.
    TagStats StatsFor(std::string_view tag) const;
    // Enumerate every outstanding allocation site (Debug/Development only).
    void ForEachSite(const void* ctx,
                     void (*fn)(const void* ctx, const SiteInfo& info)) const;

    // Returns true if any allocation is still outstanding.
    bool HasLeaks() const;
    // Logs (via LUMA_LOG_ERROR) every outstanding allocation, then clears the
    // site list. Returns the number of leaks reported.
    usize ReportLeaks();
    // Empties the site list and resets the per-tag counters to zero. Used at
    // app shutdown to make sure no future allocations touch the freed tables.
    void ResetStats();

    Allocator& Inner() { return *m_inner; }

private:
    Allocator* m_inner;
};

// The one global tracking allocator. Wraps the system heap so every LUMA_NEW
// can be observed without a per-call-site allocator argument.
TrackingAllocator& GlobalTracking();

// Convenience: in Shipping, all tracking becomes a pass-through. In
// Debug/Development, tracking is real. Lets code stay free of #ifdefs.
bool IsTrackingActive();

}  // namespace Luma::Memory
