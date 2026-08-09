#pragma once

#include <cstddef>
#include <cstdint>

// Fixed-width scalar aliases used throughout the engine. Prefer these over the
// built-in types so sizes are explicit and consistent across platforms.

namespace Luma {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;
using isize = std::ptrdiff_t;

using uptr = std::uintptr_t;
using iptr = std::intptr_t;

using byte = std::uint8_t;

static_assert(sizeof(f32) == 4, "f32 must be 32-bit");
static_assert(sizeof(f64) == 8, "f64 must be 64-bit");

}  // namespace Luma

// Utility macros.
#define LUMA_UNUSED(x) ((void)(x))

#if defined(_MSC_VER)
#define LUMA_DEBUGBREAK() __debugbreak()
#define LUMA_FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define LUMA_DEBUGBREAK() __builtin_trap()
#define LUMA_FORCEINLINE inline __attribute__((always_inline))
#else
#define LUMA_DEBUGBREAK() ((void)0)
#define LUMA_FORCEINLINE inline
#endif
