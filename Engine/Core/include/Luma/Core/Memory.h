#pragma once

#include <new>
#include <type_traits>

// One-stop include for Luma's memory subsystem. Pulls in the Allocator
// interface, the concrete allocators (Arena, Pool), and the tracking layer
// that powers the shutdown leak report.

#include "Luma/Core/Alloc.h"
#include "Luma/Core/Arena.h"
#include "Luma/Core/Pool.h"
#include "Luma/Core/Track.h"
#include "Luma/Core/Assert.h"

// --- Convenience macros -----------------------------------------------------
// LUMA_NEW / LUMA_DELETE route through the global tracking allocator so the
// leak report at shutdown can see them. Use these anywhere you'd otherwise
// write `new T(...)` / `delete p` for engine-owned objects.
//
//   auto* e = LUMA_NEW(MyEngineTag, Entity)();  // calls ctor
//   LUMA_DELETE(MyEngineTag, e);
//   e = nullptr;
//
// For the per-frame scratch arena:
//   int* temp = LUMA_SCRATCH_NEW(int, 32);  // 32 ints, freed next frame
//   LUMA_SCRATCH_DELETE(int, temp);          // optional; freed by ResetFrameScratch
//
// In Shipping builds the macros call the system allocator directly; in Debug
// and Development they go through the global TrackingAllocator.


#define LUMA_ALLOC_DETAIL_SITE(tag)                               \
    ::Luma::Memory::AllocationSite{__FILE__,                      \
        static_cast<const char*>(__func__), __LINE__, (tag)}

// Allocate one T, calling its constructor. `tag` must be a string literal.
//   T* obj = LUMA_NEW(MyTag, T)(ctor args...);
#define LUMA_NEW(tag, T)                                          \
    ::new (::Luma::Memory::Alloc(                                 \
        ::Luma::Memory::GlobalTracking(), sizeof(T), alignof(T),  \
        LUMA_ALLOC_DETAIL_SITE(tag))) T

// Free one T, calling its destructor.
#define LUMA_DELETE(tag, ptr)                                     \
    do {                                                          \
        using _T = std::remove_pointer_t<decltype(ptr)>;          \
        if (ptr) {                                                \
            (ptr)->~_T();                                         \
            ::Luma::Memory::Free(                                 \
                ::Luma::Memory::GlobalTracking(), (ptr), sizeof(_T), \
                LUMA_ALLOC_DETAIL_SITE(tag));                     \
            (ptr) = nullptr;                                      \
        }                                                         \
    } while (0)

// Per-frame scratch helpers. Reset every frame by the engine; you do not need
// to free these manually except to release memory mid-frame (use an
// ArenaScope around the block).
#define LUMA_SCRATCH_NEW(T, count)                                \
    static_cast<T*>(::Luma::Memory::AllocZero(                    \
        ::Luma::Memory::FrameScratch(),                           \
        sizeof(T) * (count), alignof(T), LUMA_ALLOC_DETAIL_SITE("Scratch")))

#define LUMA_SCRATCH_DELETE(T, ptr)                               \
    do {                                                          \
        if (ptr) {                                                \
            ::Luma::Memory::Free(                                 \
                ::Luma::Memory::FrameScratch(), (ptr),            \
                sizeof(T), LUMA_ALLOC_DETAIL_SITE("Scratch"));    \
            (ptr) = nullptr;                                      \
        }                                                         \
    } while (0)
