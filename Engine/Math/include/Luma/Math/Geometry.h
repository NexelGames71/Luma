#pragma once

#include <cmath>
#include <limits>

#include "Luma/Math/Math.h"

// Bounding volumes, rays, planes and their intersection tests — the geometry
// primitives the engine needs for picking (ray vs scene), culling (frustum vs
// AABB), and spatial queries. All right-handed, consistent with Luma's math.

namespace Luma::Math {

struct Ray {
    Vec3 origin;
    Vec3 dir;  // expected normalized for distance results to be world units
};

struct Sphere {
    Vec3 center;
    f32 radius = 1.0f;
};

// Plane: points p with Dot(normal, p) + d == 0. Positive SignedDistance is the
// side the normal points toward.
struct Plane {
    Vec3 normal;
    f32 d = 0.0f;
    f32 SignedDistance(const Vec3& p) const { return Dot(normal, p) + d; }
};

struct AABB {
    Vec3 min;
    Vec3 max;

    bool Contains(const Vec3& p) const {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y &&
               p.z >= min.z && p.z <= max.z;
    }
    Vec3 Center() const { return (min + max) * 0.5f; }
    Vec3 Extents() const { return (max - min) * 0.5f; }
};

inline bool Overlaps(const AABB& a, const AABB& b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y &&
           a.max.y >= b.min.y && a.min.z <= b.max.z && a.max.z >= b.min.z;
}

// Slab test. On hit sets tHit to the nearest non-negative intersection distance.
inline bool RayIntersectsAABB(const Ray& r, const AABB& box, f32& tHit) {
    f32 tmin = -std::numeric_limits<f32>::infinity();
    f32 tmax = std::numeric_limits<f32>::infinity();
    const f32 o[3] = {r.origin.x, r.origin.y, r.origin.z};
    const f32 dd[3] = {r.dir.x, r.dir.y, r.dir.z};
    const f32 lo[3] = {box.min.x, box.min.y, box.min.z};
    const f32 hi[3] = {box.max.x, box.max.y, box.max.z};
    for (int i = 0; i < 3; ++i) {
        if (std::abs(dd[i]) < 1e-8f) {
            if (o[i] < lo[i] || o[i] > hi[i]) return false;  // parallel, outside
        } else {
            f32 inv = dd[i] != 0.0f ? 1.0f / dd[i] : 0.0f;  // guarded (silences C4723)
            f32 t0 = (lo[i] - o[i]) * inv;
            f32 t1 = (hi[i] - o[i]) * inv;
            if (t0 > t1) {
                f32 tmp = t0;
                t0 = t1;
                t1 = tmp;
            }
            tmin = t0 > tmin ? t0 : tmin;
            tmax = t1 < tmax ? t1 : tmax;
            if (tmin > tmax) return false;
        }
    }
    if (tmax < 0.0f) return false;  // box entirely behind the ray
    tHit = tmin >= 0.0f ? tmin : tmax;
    return true;
}

inline bool RayIntersectsSphere(const Ray& r, const Sphere& s, f32& tHit) {
    Vec3 oc = r.origin - s.center;
    f32 a = Dot(r.dir, r.dir);
    f32 b = 2.0f * Dot(oc, r.dir);
    f32 c = Dot(oc, oc) - s.radius * s.radius;
    f32 disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;
    f32 sq = std::sqrt(disc);
    f32 t0 = (-b - sq) / (2.0f * a);
    f32 t1 = (-b + sq) / (2.0f * a);
    f32 t = t0 >= 0.0f ? t0 : t1;
    if (t < 0.0f) return false;
    tHit = t;
    return true;
}

inline bool RayIntersectsPlane(const Ray& r, const Plane& p, f32& tHit) {
    f32 denom = Dot(p.normal, r.dir);
    if (std::abs(denom) < 1e-8f) return false;  // parallel
    f32 t = -(Dot(p.normal, r.origin) + p.d) / denom;
    if (t < 0.0f) return false;
    tHit = t;
    return true;
}

// View frustum as six inward-facing planes, for coarse AABB culling.
struct Frustum {
    Plane planes[6];

    // Extracts planes from a combined view-projection matrix (Gribb-Hartmann).
    // Assumes Luma's column-major storage and 0..1 clip depth (Vulkan).
    static Frustum FromViewProj(const Mat4& m) {
        // math element (row, col) is stored at m[col*4 + row].
        auto E = [&](int row, int col) { return m.m[col * 4 + row]; };
        auto makePlane = [](f32 a, f32 b, f32 c, f32 dd) {
            Plane p;
            f32 len = std::sqrt(a * a + b * b + c * c);
            if (len <= 0.0f) len = 1.0f;
            p.normal = Vec3(a / len, b / len, c / len);
            p.d = dd / len;
            return p;
        };
        Frustum f;
        // left, right, bottom, top, near, far
        f.planes[0] = makePlane(E(3, 0) + E(0, 0), E(3, 1) + E(0, 1),
                                E(3, 2) + E(0, 2), E(3, 3) + E(0, 3));
        f.planes[1] = makePlane(E(3, 0) - E(0, 0), E(3, 1) - E(0, 1),
                                E(3, 2) - E(0, 2), E(3, 3) - E(0, 3));
        f.planes[2] = makePlane(E(3, 0) + E(1, 0), E(3, 1) + E(1, 1),
                                E(3, 2) + E(1, 2), E(3, 3) + E(1, 3));
        f.planes[3] = makePlane(E(3, 0) - E(1, 0), E(3, 1) - E(1, 1),
                                E(3, 2) - E(1, 2), E(3, 3) - E(1, 3));
        f.planes[4] = makePlane(E(2, 0), E(2, 1), E(2, 2), E(2, 3));
        f.planes[5] = makePlane(E(3, 0) - E(2, 0), E(3, 1) - E(2, 1),
                                E(3, 2) - E(2, 2), E(3, 3) - E(2, 3));
        return f;
    }

    // Conservative: true unless the box is fully outside some plane.
    bool Intersects(const AABB& box) const {
        for (const Plane& p : planes) {
            Vec3 pv{p.normal.x >= 0.0f ? box.max.x : box.min.x,
                    p.normal.y >= 0.0f ? box.max.y : box.min.y,
                    p.normal.z >= 0.0f ? box.max.z : box.min.z};
            if (p.SignedDistance(pv) < 0.0f) return false;
        }
        return true;
    }
};

}  // namespace Luma::Math
