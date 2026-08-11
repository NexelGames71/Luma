#pragma once

#include <cmath>

#include "Luma/Core/Types.h"

// Minimal column-major 3D math for the renderer (Vulkan clip space: depth 0..1,
// framebuffer Y-down handled by flipping the projection). A fuller Math module
// grows from here.

namespace Luma::Math {

constexpr f32 kPi = 3.14159265358979323846f;
constexpr f32 Radians(f32 degrees) { return degrees * (kPi / 180.0f); }

struct Vec3 {
    f32 x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(f32 x_, f32 y_, f32 z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(f32 s) const { return {x * s, y * s, z * s}; }
};

inline f32 Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}
inline Vec3 Normalize(const Vec3& v) {
    f32 len = std::sqrt(Dot(v, v));
    return len > 0.0f ? v * (1.0f / len) : v;
}

// Column-major 4x4: element (row r, col c) at m[c*4 + r].
struct Mat4 {
    f32 m[16] = {0};

    static Mat4 Identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c) {
            for (int row = 0; row < 4; ++row) {
                f32 sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += m[k * 4 + row] * b.m[c * 4 + k];
                }
                r.m[c * 4 + row] = sum;
            }
        }
        return r;
    }
};

inline Mat4 Translate(const Vec3& t) {
    Mat4 r = Mat4::Identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

inline Mat4 Scale(const Vec3& s) {
    Mat4 r = Mat4::Identity();
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
}

inline Mat4 RotateY(f32 a) {
    Mat4 r = Mat4::Identity();
    f32 c = std::cos(a), s = std::sin(a);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

inline Mat4 RotateX(f32 a) {
    Mat4 r = Mat4::Identity();
    f32 c = std::cos(a), s = std::sin(a);
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

inline Mat4 RotateZ(f32 a) {
    Mat4 r = Mat4::Identity();
    f32 c = std::cos(a), s = std::sin(a);
    r.m[0] = c;
    r.m[1] = s;
    r.m[4] = -s;
    r.m[5] = c;
    return r;
}

inline Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = Normalize(center - eye);
    Vec3 s = Normalize(Cross(f, up));
    Vec3 u = Cross(s, f);
    Mat4 r = Mat4::Identity();
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8] = s.z;
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -Dot(s, eye);
    r.m[13] = -Dot(u, eye);
    r.m[14] = Dot(f, eye);
    return r;
}

// Vulkan perspective: depth 0..1, with the Y axis flipped for the framebuffer.
inline Mat4 Perspective(f32 fovYRadians, f32 aspect, f32 nearZ, f32 farZ) {
    f32 t = std::tan(fovYRadians * 0.5f);
    Mat4 r;
    r.m[0] = 1.0f / (aspect * t);
    r.m[5] = -1.0f / t;  // flip Y for Vulkan
    r.m[10] = farZ / (nearZ - farZ);
    r.m[11] = -1.0f;
    r.m[14] = (nearZ * farZ) / (nearZ - farZ);
    return r;
}

}  // namespace Luma::Math
