#include "Luma/Mesh/Mesh.h"

#include <cmath>

namespace Luma {
namespace {

using Math::Vec3;

// Cube: 24 vertices (4 per face) so each face gets a flat outward normal.
MeshData BuildCube() {
    MeshData m;
    struct Face {
        Vec3 normal, u, v;
    };
    // Each face: outward normal + the two in-plane axes spanning it.
    const Face faces[6] = {
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},    // +Z
        {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},  // -Z
        {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},   // +X
        {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},   // -X
        {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},   // +Y
        {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},   // -Y
    };
    for (const Face& f : faces) {
        u32 base = static_cast<u32>(m.vertices.size());
        for (int i = 0; i < 4; ++i) {
            f32 su = (i == 1 || i == 2) ? 0.5f : -0.5f;
            f32 sv = (i >= 2) ? 0.5f : -0.5f;
            Vec3 p = f.normal * 0.5f + f.u * su + f.v * sv;
            m.vertices.push_back({p, f.normal});
        }
        m.indices.insert(m.indices.end(),
                         {base, base + 1, base + 2, base + 2, base + 3, base});
    }
    return m;
}

// Unit plane (1x1) on the XZ plane, normal +Y.
MeshData BuildPlane() {
    MeshData m;
    Vec3 n{0, 1, 0};
    m.vertices = {{{-0.5f, 0, -0.5f}, n},
                  {{0.5f, 0, -0.5f}, n},
                  {{0.5f, 0, 0.5f}, n},
                  {{-0.5f, 0, 0.5f}, n}};
    // CCW when viewed from +Y so the lit face points up (matches the +Y normal).
    m.indices = {0, 2, 1, 0, 3, 2};
    return m;
}

// UV sphere, radius 0.5.
MeshData BuildSphere() {
    MeshData m;
    const int rings = 24, sectors = 32;
    const f32 pi = 3.14159265358979323846f;
    for (int r = 0; r <= rings; ++r) {
        f32 phi = pi * static_cast<f32>(r) / rings;  // 0..pi
        for (int s = 0; s <= sectors; ++s) {
            f32 theta = 2.0f * pi * static_cast<f32>(s) / sectors;
            Vec3 nrm{std::sin(phi) * std::cos(theta), std::cos(phi),
                     std::sin(phi) * std::sin(theta)};
            m.vertices.push_back({nrm * 0.5f, nrm});
        }
    }
    int stride = sectors + 1;
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            u32 a = static_cast<u32>(r * stride + s);
            u32 b = static_cast<u32>((r + 1) * stride + s);
            m.indices.insert(m.indices.end(),
                             {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    return m;
}

// Cylinder, radius 0.5, height 1, axis +Y, with caps.
MeshData BuildCylinder() {
    MeshData m;
    const int sectors = 32;
    const f32 pi = 3.14159265358979323846f;
    const f32 h = 0.5f;
    // Side wall.
    for (int s = 0; s <= sectors; ++s) {
        f32 theta = 2.0f * pi * static_cast<f32>(s) / sectors;
        Vec3 nrm{std::cos(theta), 0.0f, std::sin(theta)};
        m.vertices.push_back({{nrm.x * 0.5f, h, nrm.z * 0.5f}, nrm});
        m.vertices.push_back({{nrm.x * 0.5f, -h, nrm.z * 0.5f}, nrm});
    }
    for (int s = 0; s < sectors; ++s) {
        u32 a = static_cast<u32>(s * 2);
        m.indices.insert(m.indices.end(),
                         {a, a + 1, a + 2, a + 2, a + 1, a + 3});
    }
    // Caps (flat normals).
    auto cap = [&](f32 y, Vec3 nrm, bool flip) {
        u32 center = static_cast<u32>(m.vertices.size());
        m.vertices.push_back({{0, y, 0}, nrm});
        u32 first = static_cast<u32>(m.vertices.size());
        for (int s = 0; s <= sectors; ++s) {
            f32 theta = 2.0f * pi * static_cast<f32>(s) / sectors;
            m.vertices.push_back(
                {{std::cos(theta) * 0.5f, y, std::sin(theta) * 0.5f}, nrm});
        }
        for (int s = 0; s < sectors; ++s) {
            u32 a = first + static_cast<u32>(s);
            if (flip)
                m.indices.insert(m.indices.end(), {center, a + 1, a});
            else
                m.indices.insert(m.indices.end(), {center, a, a + 1});
        }
    };
    cap(h, {0, 1, 0}, false);
    cap(-h, {0, -1, 0}, true);
    return m;
}

}  // namespace

MeshData BuildPrimitive(MeshPrimitive primitive) {
    switch (primitive) {
        case MeshPrimitive::Plane: return BuildPlane();
        case MeshPrimitive::Sphere: return BuildSphere();
        case MeshPrimitive::Cylinder: return BuildCylinder();
        case MeshPrimitive::Cube:
        default: return BuildCube();
    }
}

}  // namespace Luma
