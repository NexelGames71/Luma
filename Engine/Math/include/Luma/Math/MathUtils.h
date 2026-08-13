#pragma once

#include <cmath>

#include "Luma/Math/Math.h"

// Scalar/vector convenience helpers that build on the core Vec3/Mat4 in Math.h.

namespace Luma::Math {

inline f32 Clamp(f32 v, f32 lo, f32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
inline f32 Saturate(f32 v) { return Clamp(v, 0.0f, 1.0f); }

inline f32 Min(f32 a, f32 b) { return a < b ? a : b; }
inline f32 Max(f32 a, f32 b) { return a > b ? a : b; }

inline f32 Lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
inline Vec3 Lerp(const Vec3& a, const Vec3& b, f32 t) {
    return a + (b - a) * t;
}

inline f32 Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }
inline f32 Distance(const Vec3& a, const Vec3& b) { return Length(b - a); }

inline f32 Degrees(f32 radians) { return radians * (180.0f / kPi); }

}  // namespace Luma::Math
