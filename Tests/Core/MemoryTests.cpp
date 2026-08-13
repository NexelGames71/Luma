#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Luma/Core/Assert.h"
#include "Luma/Core/Memory.h"

using namespace Luma;
using namespace Luma::Memory;

namespace {

// RAII helper: ensures a global tracking snapshot is reset around a test so
// tags from one case don't bleed into another.
struct TrackingScope {
    TrackingScope() {
        if (IsTrackingActive()) GlobalTracking().ResetStats();
    }
    ~TrackingScope() {
        if (IsTrackingActive()) GlobalTracking().ResetStats();
    }
};

}  // namespace

// -- Allocator interface ----------------------------------------------------

TEST_CASE("SystemHeap alloc/free round trip", "[memory][heap]") {
    TrackingScope ts;
    auto& a = SystemHeap();
    void* p = a.Alloc(64, 16);
    REQUIRE(p != nullptr);
    REQUIRE(reinterpret_cast<uptr>(p) % 16 == 0);
    std::memset(p, 0xAB, 64);
    a.Free(p, 64);
    SUCCEED();
}

TEST_CASE("SystemHeap AllocZero is zero-initialized", "[memory][heap]") {
    auto& a = SystemHeap();
    void* p = a.AllocZero(256, 16);
    REQUIRE(p != nullptr);
    auto* bytes = static_cast<byte*>(p);
    for (usize i = 0; i < 256; ++i) REQUIRE(bytes[i] == 0);
    a.Free(p, 256);
}

TEST_CASE("Free of nullptr is a no-op", "[memory][heap]") {
    auto& a = SystemHeap();
    a.Free(nullptr);  // must not crash
    SUCCEED();
}

// -- Arena ------------------------------------------------------------------

TEST_CASE("Arena basic alloc respects alignment", "[memory][arena]") {
    alignas(64) byte buf[1024];
    Arena arena(buf, sizeof(buf));

    void* a1 = arena.Alloc(10, 1);
    (void)a1;  // touch: exercises the unaligned alloc path
    void* a2 = arena.Alloc(10, 64);
    REQUIRE(reinterpret_cast<uptr>(a2) % 64 == 0);
    void* a3 = arena.AllocZero(100, 16);
    REQUIRE(a3 != nullptr);
    auto* b = static_cast<byte*>(a3);
    for (usize i = 0; i < 100; ++i) REQUIRE(b[i] == 0);

    REQUIRE(arena.Used() > 100);
    REQUIRE(arena.PeakUsed() == arena.Used());
    REQUIRE(arena.AllocationCount() == 3);
}

TEST_CASE("Arena Reset frees everything at once", "[memory][arena]") {
    alignas(16) byte buf[256];
    Arena arena(buf, sizeof(buf));
    arena.Alloc(64);
    arena.Alloc(64);
    REQUIRE(arena.Used() > 0);
    arena.Reset();
    REQUIRE(arena.Used() == 0);
    REQUIRE(arena.AllocationCount() == 0);
    // Peak is preserved across Reset (it's a high-water mark).
    REQUIRE(arena.PeakUsed() > 0);
}

TEST_CASE("Arena Marker/RestoreTo is stack-disciplined", "[memory][arena]") {
    alignas(16) byte buf[1024];
    Arena arena(buf, sizeof(buf));
    arena.Alloc(32);  // permanent
    usize marker = arena.Marker();
    arena.Alloc(64);  // scratch region
    arena.Alloc(64);
    REQUIRE(arena.Used() > 32 + 64);
    arena.RestoreTo(marker);
    REQUIRE(arena.Used() == 32);
    // Re-alloc into the same region: should succeed and not OOM.
    void* p = arena.Alloc(64, 16);
    REQUIRE(p != nullptr);
}

TEST_CASE("ArenaScope restores on scope exit", "[memory][arena]") {
    alignas(16) byte buf[1024];
    Arena arena(buf, sizeof(buf));
    arena.Alloc(16);  // baseline
    {
        ArenaScope scope(arena);
        arena.Alloc(128);
        arena.Alloc(128);
        REQUIRE(arena.Used() > 16);
    }
    REQUIRE(arena.Used() == 16);
}

TEST_CASE("Arena self-allocated buffer uses backing allocator", "[memory][arena]") {
    TrackingScope ts;
    Arena arena(1024, SystemHeap());
    void* p = arena.Alloc(512, 16);
    REQUIRE(p != nullptr);
    // The arena owns the buffer; it'll be freed in the dtor.
}

TEST_CASE("Arena OOM asserts when capacity is exceeded", "[memory][arena]") {
    alignas(16) byte buf[64];
    Arena arena(buf, sizeof(buf));
    // The default assert handler aborts; install a swallowing one for the
    // duration of this test so we can confirm the soft-fail (nullptr) path.
    auto prev = Luma::Detail::SetAssertHandler(
        [](const Luma::AssertInfo&) {});
    void* p = arena.Alloc(1024, 16);
    Luma::Detail::SetAssertHandler(prev);
    REQUIRE(p == nullptr);
}

// -- Pool -------------------------------------------------------------------

TEST_CASE("Pool hands out blocks and recycles them", "[memory][pool]") {
    Pool pool(/*block_size*/ 32, /*block_count*/ 4, SystemHeap());
    REQUIRE(pool.Used() == 0);
    REQUIRE(pool.Capacity() == 4);

    void* a = pool.Alloc(32);
    void* b = pool.Alloc(32);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a != b);
    REQUIRE(pool.Used() == 2);
    REQUIRE(pool.PeakUsed() == 2);

    pool.Free(a, 32);
    REQUIRE(pool.Used() == 1);
    void* c = pool.Alloc(32);
    REQUIRE(c != nullptr);
    // After free+alloc, c should reuse the freed slot (or any free block).
    REQUIRE(pool.Used() == 2);
    pool.Free(b, 32);
    pool.Free(c, 32);
    REQUIRE(pool.Used() == 0);
}

TEST_CASE("Pool AllocZero zeros the block", "[memory][pool]") {
    Pool pool(64, 2, SystemHeap());
    void* p = pool.AllocZero(64);
    REQUIRE(p != nullptr);
    auto* b = static_cast<byte*>(p);
    for (usize i = 0; i < 64; ++i) REQUIRE(b[i] == 0);
    pool.Free(p, 64);
}

// -- Tracking + leak report -------------------------------------------------

TEST_CASE("TrackingAllocator records sites and frees correctly", "[memory][track]") {
    if (!IsTrackingActive()) {
        WARN("Tracking is off in this build; skipping.");
        return;
    }
    TrackingScope ts;

    const AllocationSite siteA1{"a.cpp", "fn1", 10, "TagA"};
    const AllocationSite siteA2{"a.cpp", "fn2", 20, "TagA"};
    const AllocationSite siteB1{"b.cpp", "fn3", 30, "TagB"};
    void* p1 = Alloc(GlobalTracking(), 32, 16, siteA1);
    void* p2 = Alloc(GlobalTracking(), 64, 16, siteA2);
    void* p3 = Alloc(GlobalTracking(), 128, 16, siteB1);
    REQUIRE(p1 != nullptr);
    REQUIRE(p2 != nullptr);
    REQUIRE(p3 != nullptr);

    TagStats aStats = GlobalTracking().StatsFor("TagA");
    TagStats bStats = GlobalTracking().StatsFor("TagB");
    REQUIRE(aStats.currentBytes == 32 + 64);
    REQUIRE(aStats.currentCount == 2);
    REQUIRE(bStats.currentBytes == 128);
    REQUIRE(bStats.currentCount == 1);
    REQUIRE(aStats.peakBytes >= aStats.currentBytes);

    Free(GlobalTracking(), p1, 32, siteA1);
    aStats = GlobalTracking().StatsFor("TagA");
    REQUIRE(aStats.currentBytes == 64);
    REQUIRE(aStats.currentCount == 1);

    Free(GlobalTracking(), p2, 64, siteA2);
    Free(GlobalTracking(), p3, 128, siteB1);
    REQUIRE(GlobalTracking().StatsFor("TagA").currentCount == 0);
    REQUIRE(GlobalTracking().StatsFor("TagB").currentCount == 0);
    REQUIRE_FALSE(GlobalTracking().HasLeaks());
}

TEST_CASE("ReportLeaks dumps outstanding sites", "[memory][track]") {
    if (!IsTrackingActive()) {
        WARN("Tracking is off in this build; skipping.");
        return;
    }
    TrackingScope ts;

    void* p = Alloc(GlobalTracking(), 256, 16,
                    AllocationSite{"leak_test.cpp", "ReportLeaks", 1, "LeakTag"});
    REQUIRE(p != nullptr);

    REQUIRE(GlobalTracking().HasLeaks());
    usize n = GlobalTracking().ReportLeaks();
    REQUIRE(n == 1);

    // Clean up so we don't trip the Application destructor leak dump in CI.
    Free(GlobalTracking(), p, 256,
         AllocationSite{"leak_test.cpp", "ReportLeaks", 1, "LeakTag"});
    REQUIRE_FALSE(GlobalTracking().HasLeaks());
}

// -- LUMA_NEW / LUMA_DELETE macros -----------------------------------------

struct TrackerProbe {
    static std::atomic<int> alive;
    int payload = 0;
    TrackerProbe() { alive.fetch_add(1); }
    explicit TrackerProbe(int v) : payload(v) { alive.fetch_add(1); }
    ~TrackerProbe() { alive.fetch_sub(1); }
};
std::atomic<int> TrackerProbe::alive{0};

TEST_CASE("LUMA_NEW runs ctor and LUMA_DELETE runs dtor", "[memory][macros]") {
    TrackingScope ts;
    REQUIRE(TrackerProbe::alive.load() == 0);

    auto* p = LUMA_NEW("ProbeTag", TrackerProbe)(7);
    REQUIRE(p != nullptr);
    REQUIRE(p->payload == 7);
    REQUIRE(TrackerProbe::alive.load() == 1);

    LUMA_DELETE("ProbeTag", p);
    REQUIRE(p == nullptr);
    REQUIRE(TrackerProbe::alive.load() == 0);
}

TEST_CASE("LUMA_DELETE null is a no-op", "[memory][macros]") {
    TrackingScope ts;
    TrackerProbe* p = nullptr;
    LUMA_DELETE("ProbeTag", p);  // must not crash
    REQUIRE(p == nullptr);
    REQUIRE(TrackerProbe::alive.load() == 0);
}

// -- Frame scratch ----------------------------------------------------------

namespace {
// Helper as a free function to keep the macro test bodies tidy.
usize after_alloc_used(Arena& a) { return a.Used(); }
}  // namespace

TEST_CASE("FrameScratch serves and resets per frame", "[memory][scratch]") {
    Arena& scratch = FrameScratch();
    REQUIRE(scratch.Capacity() >= 1024);

    int* a = LUMA_SCRATCH_NEW(int, 16);
    int* b = LUMA_SCRATCH_NEW(int, 32);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    // Verify usable (write to it).
    for (int i = 0; i < 16; ++i) a[i] = i;
    REQUIRE(after_alloc_used(scratch) > 0);

    ResetFrameScratch();
    REQUIRE(scratch.Used() == 0);
    // After reset the next alloc returns a fresh block from the same arena.
    int* c = LUMA_SCRATCH_NEW(int, 16);
    REQUIRE(c != nullptr);
    ResetFrameScratch();
}

// -- Thread-safety smoke test ----------------------------------------------

TEST_CASE("TrackingAllocator is thread-safe under concurrent alloc/free",
          "[memory][track][threads]") {
    if (!IsTrackingActive()) {
        WARN("Tracking is off in this build; skipping.");
        return;
    }
    TrackingScope ts;

    constexpr int kThreads = 4;
    constexpr int kIters = 64;
    std::vector<std::thread> threads;
    std::vector<void*> ptrs;
    std::mutex ptrsMu;

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t]() {
            const AllocationSite site{"thread_test.cpp", "thread_fn", 1, "ThreadTag"};
            for (int i = 0; i < kIters; ++i) {
                void* p = Alloc(GlobalTracking(), 32, 16, site);
                REQUIRE(p != nullptr);
                {
                    std::lock_guard<std::mutex> lock(ptrsMu);
                    ptrs.push_back(p);
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    TagStats s = GlobalTracking().StatsFor("ThreadTag");
    REQUIRE(s.currentCount == static_cast<usize>(kThreads * kIters));

    for (void* p : ptrs) {
        Free(GlobalTracking(), p, 32,
             AllocationSite{"thread_test.cpp", "thread_fn", 1, "ThreadTag"});
    }
    REQUIRE(GlobalTracking().StatsFor("ThreadTag").currentCount == 0);
}
