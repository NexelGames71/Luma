#include "Luma/Core/Track.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Luma/Core/Assert.h"
#include "Luma/Core/Log.h"

namespace Luma::Memory {

namespace {

// -- Internals ---------------------------------------------------------------

struct SiteKey {
    void* ptr;
    bool operator==(const SiteKey& o) const { return ptr == o.ptr; }
};
struct SiteKeyHash {
    usize operator()(const SiteKey& k) const {
        return std::hash<void*>{}(k.ptr);
    }
};

struct TagBucket {
    TagStats stats{};
    std::vector<SiteInfo> sites;
};

// One global registry; guarded by a mutex so it can be touched from any
// thread. The map is keyed by the tag string-literal address (not a copy), so
// callers must use a string literal (the LUMA_NEW macro enforces this).
struct Registry {
    std::mutex mu;
    std::unordered_map<const char*, TagBucket> byTag;
    usize totalAllocs = 0;
    usize totalFrees = 0;

    TagBucket& BucketFor(const char* tag) {
        auto it = byTag.find(tag);
        if (it == byTag.end()) {
            TagBucket b{};
            b.stats.tag = tag ? tag : "";
            it = byTag.emplace(tag, std::move(b)).first;
        }
        return it->second;
    }
};

Registry& GetRegistry() {
    static Registry r;
    return r;
}

}  // namespace

// -- TrackingAllocator impl --------------------------------------------------
// (Default ctor is defined in the header.)

TrackingAllocator::~TrackingAllocator() {
    // Don't auto-report; the explicit ReportLeaks() does. The destructor just
    // tears down state.
}

void* TrackingAllocator::Alloc(usize size, usize align, const AllocationSite& site) {
    void* p = m_inner->Alloc(size, align, site);
    if (!p) return nullptr;

    if (IsTrackingActive()) {
        auto& reg = GetRegistry();
        std::lock_guard<std::mutex> lock(reg.mu);
        // Tag precedence: explicit tag on the site > function name > "<alloc>".
        // LUMA_NEW always sets tag; raw Alloc() callers get the function name
        // so their allocations are at least grouped.
        const char* tag = site.tag;
        if (!tag || tag[0] == '\0') {
            tag = site.function ? site.function : "<alloc>";
        }
        auto& bucket = reg.BucketFor(tag);
        SiteInfo info{p, size, tag, site.file, site.function, site.line};
        bucket.sites.push_back(info);
        bucket.stats.currentBytes += size;
        bucket.stats.currentCount += 1;
        if (bucket.stats.currentBytes > bucket.stats.peakBytes) {
            bucket.stats.peakBytes = bucket.stats.currentBytes;
        }
        if (bucket.stats.currentCount > bucket.stats.peakCount) {
            bucket.stats.peakCount = bucket.stats.currentCount;
        }
        ++bucket.stats.totalAllocs;
        ++reg.totalAllocs;
    }
    return p;
}

void* TrackingAllocator::AllocZero(usize size, usize align, const AllocationSite& site) {
    void* p = Alloc(size, align, site);
    if (p) std::memset(p, 0, size);
    return p;
}

void TrackingAllocator::Free(void* p, usize size, const AllocationSite& site) {
    if (!p) return;
    if (IsTrackingActive()) {
        auto& reg = GetRegistry();
        std::lock_guard<std::mutex> lock(reg.mu);
        // Find the site. The stand-in tag we used in Alloc is the function
        // name; we don't have it on Free, so we do a linear scan over all
        // tags. Acceptable: site counts are small in the typical case, and
        // the leak-report path is offline.
        for (auto& [tag, bucket] : reg.byTag) {
            for (auto it = bucket.sites.begin(); it != bucket.sites.end(); ++it) {
                if (it->ptr == p) {
                    if (it->size >= size) {
                        bucket.stats.currentBytes -= it->size;
                    } else {
                        bucket.stats.currentBytes -= size;
                    }
                    bucket.stats.currentCount -= 1;
                    ++bucket.stats.totalFrees;
                    ++reg.totalFrees;
                    bucket.sites.erase(it);
                    m_inner->Free(p, size, site);
                    return;
                }
            }
        }
        LUMA_ASSERT(false, "TrackingAllocator::Free of unknown pointer (leak or foreign ptr?)");
    }
    m_inner->Free(p, size, site);
}

TagStats TrackingAllocator::StatsFor(std::string_view tag) const {
    auto& reg = GetRegistry();
    std::lock_guard<std::mutex> lock(reg.mu);
    // The registry is keyed by string-literal pointer (cheap pointer-equal
    // lookup at Alloc time). The public StatsFor() takes a string_view so
    // callers don't have to fish out the literal — we do a content scan
    // instead. The number of tags is tiny in practice.
    for (const auto& [key, bucket] : reg.byTag) {
        if (key && tag == key) return bucket.stats;
    }
    return TagStats{tag};
}

void TrackingAllocator::ForEachSite(
    const void* ctx, void (*fn)(const void*, const SiteInfo&)) const {
    auto& reg = GetRegistry();
    std::lock_guard<std::mutex> lock(reg.mu);
    for (auto& [tag, bucket] : reg.byTag) {
        for (const auto& s : bucket.sites) fn(ctx, s);
    }
}

bool TrackingAllocator::HasLeaks() const {
    auto& reg = GetRegistry();
    std::lock_guard<std::mutex> lock(reg.mu);
    for (auto& [tag, bucket] : reg.byTag) {
        if (!bucket.sites.empty()) return true;
    }
    return false;
}

usize TrackingAllocator::ReportLeaks() {
    auto& reg = GetRegistry();
    std::lock_guard<std::mutex> lock(reg.mu);

    usize totalLeaks = 0;
    usize totalBytes = 0;
    std::vector<SiteInfo> all;
    for (auto& [tag, bucket] : reg.byTag) {
        for (const auto& s : bucket.sites) {
            all.push_back(s);
            totalBytes += s.size;
        }
        totalLeaks += bucket.sites.size();
    }
    if (totalLeaks == 0) {
        LUMA_LOG_INFO("Memory", "leak report: 0 outstanding allocations");
        return 0;
    }
    // Sort by size desc to make the worst offenders jump out.
    std::sort(all.begin(), all.end(),
              [](const SiteInfo& a, const SiteInfo& b) { return a.size > b.size; });
    LUMA_LOG_ERROR("Memory",
                   "leak report: {} outstanding allocation(s), {} bytes total",
                   totalLeaks, totalBytes);
    for (const auto& s : all) {
        LUMA_LOG_ERROR("Memory",
                       "  leak: {} bytes tag='{}' at {} ({}:{})",
                       s.size, s.tag, s.function ? s.function : "?",
                       s.file ? s.file : "?", s.line);
    }
    return totalLeaks;
}

void TrackingAllocator::ResetStats() {
    auto& reg = GetRegistry();
    std::lock_guard<std::mutex> lock(reg.mu);
    reg.byTag.clear();
    reg.totalAllocs = 0;
    reg.totalFrees = 0;
}

// -- Global singletons + shipping switch -------------------------------------

namespace {
class GlobalTrackingInstance : public TrackingAllocator {
public:
    GlobalTrackingInstance() : TrackingAllocator(SystemHeap()) {}
};
}  // namespace

TrackingAllocator& GlobalTracking() {
    static GlobalTrackingInstance g;
    return g;
}

bool IsTrackingActive() {
#if defined(LUMA_CONFIG_SHIPPING)
    return false;
#else
    return true;
#endif
}

}  // namespace Luma::Memory
