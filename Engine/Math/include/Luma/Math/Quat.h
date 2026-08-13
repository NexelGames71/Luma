#pragma once

#include <cmath>

#include "Luma/Math/Math.h"

// Unit quaternion for 3D rotations. Layout is (x, y, z, w) with w the scalar
// part. Right-handed, matching the rest of Luma's math (a positive angle about
// +Y sends +X toward -Z). Use for smooth interpolation (Slerp) and gimbal-free
// orientation; convert to Mat4 to feed the renderer.

namespace Luma::Math {

struct Quat {
    f32 x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;

    Quat() = default;
    Quat(f32 x_, f32 y_, f32 z_, f32 w_) : x(x_), y(y_), z(z_), w(w_) {}

    static Quat Identity() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

    static Quat FromAxisAngle(const Vec3& axis, f32 radians) {
        Vec3 a = Normalize(axis);
        f32 half = radians * 0.5f;
        f32 s = std::sin(half);
        return {a.x * s, a.y * s, a.z * s, std::cos(half)};
    }

    // Hamilton product: applying (this * o) rotates by o, then by this.
    Quat operator*(const Quat& o) const {
        return {w * o.x + x * o.w + y * o.z - z * o.y,
                w * o.y - x * o.z + y * o.w + z * o.x,
                w * o.z + x * o.y - y * o.x + z * o.w,
                w * o.w - x * o.x - y * o.y - z * o.z};
    }

    // Rotate a vector: v + 2w(u x v) + 2(u x (u x v)), u = (x,y,z).
    Vec3 Rotate(const Vec3& v) const {
        Vec3 u{x, y, z};
        Vec3 t = Cross(u, v) * 2.0f;
        return v + (t * w) + Cross(u, t);
    }

    Mat4 ToMat4() const {
        Mat4 r = Mat4::Identity();
        f32 xx = x * x, yy = y * y, zz = z * z;
        f32 xy = x * y, xz = x * z, yz = y * z;
        f32 wx = w * x, wy = w * y, wz = w * z;
        // Column-major: element (row, col) at m[col*4 + row].
        r.m[0] = 1.0f - 2.0f * (yy + zz);
        r.m[1] = 2.0f * (xy + wz);
        r.m[2] = 2.0f * (xz - wy);
        r.m[4] = 2.0f * (xy - wz);
        r.m[5] = 1.0f - 2.0f * (xx + zz);
        r.m[6] = 2.0f * (yz + wx);
        r.m[8] = 2.0f * (xz + wy);
        r.m[9] = 2.0f * (yz - wx);
        r.m[10] = 1.0f - 2.0f * (xx + yy);
        return r;
    }
};

inline f32 Dot(const Quat& a, const Quat& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

inline Quat Normalize(const Quat& q) {
    f32 len = std::sqrt(Dot(q, q));
    if (len <= 0.0f) return Quat::Identity();
    f32 inv = 1.0f / len;
    return {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}

inline Quat Slerp(Quat a, Quat b, f32 t) {
    f32 d = Dot(a, b);
    if (d < 0.0f) {  // take the shorter arc
        b = Quat{-b.x, -b.y, -b.z, -b.w};
        d = -d;
    }
    if (d > 0.9995f) {  // nearly parallel: linear + renormalize
        Quat r{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
               a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
        return Normalize(r);
    }
    f32 theta0 = std::acos(d);
    f32 theta = theta0 * t;
    f32 sin0 = std::sin(theta0);
    f32 sa = std::sin(theta0 - theta) / sin0;
    f32 sb = std::sin(theta) / sin0;
    return Quat{a.x * sa + b.x * sb, a.y * sa + b.y * sb, a.z * sa + b.z * sb,
                a.w * sa + b.w * sb};
}

}  // namespace Luma::Math
