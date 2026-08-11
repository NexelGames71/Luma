#include "Luma/Grid/Grid.h"

#include <cmath>

namespace Luma {
namespace {

f32 SmoothStep(f32 edge0, f32 edge1, f32 x) {
    if (edge1 <= edge0) return x < edge0 ? 0.0f : 1.0f;
    f32 t = (x - edge0) / (edge1 - edge0);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

Math::Vec3 Lerp(const Math::Vec3& a, const Math::Vec3& b, f32 t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t};
}

}  // namespace

void Grid::Build(const Math::Vec3& focus, const GridConfig& cfg) {
    // Snap the center to the cell size so the lines don't swim as the camera
    // moves.
    Math::Vec3 center{std::round(focus.x / cfg.spacing) * cfg.spacing, 0.0f,
                      std::round(focus.z / cfg.spacing) * cfg.spacing};
    if (m_built && center.x == m_center.x && center.z == m_center.z) return;
    m_center = center;
    m_built = true;

    m_lines.clear();
    const f32 extent = static_cast<f32>(cfg.halfExtent) * cfg.spacing;
    const int segs = cfg.segments > 0 ? cfg.segments : 1;

    // Colors a point by its planar distance from the grid center (fade -> bg).
    auto shade = [&](const Math::Vec3& p, const Math::Vec3& base) {
        f32 dx = p.x - center.x;
        f32 dz = p.z - center.z;
        f32 d = std::sqrt(dx * dx + dz * dz);
        f32 f = SmoothStep(cfg.fadeStart, cfg.fadeEnd, d);
        return Lerp(base, cfg.fadeColor, f);
    };

    auto lineColor = [&](i32 i, const Math::Vec3& axisColor) {
        if (i == 0) return axisColor;
        if (cfg.majorEvery > 0 && (i % cfg.majorEvery) == 0) {
            return cfg.majorColor;
        }
        return cfg.minorColor;
    };

    // Emits one grid line (tessellated) so the color can fade along its length.
    auto emitLine = [&](const Math::Vec3& a, const Math::Vec3& b,
                        const Math::Vec3& base) {
        for (int s = 0; s < segs; ++s) {
            f32 t0 = static_cast<f32>(s) / segs;
            f32 t1 = static_cast<f32>(s + 1) / segs;
            Math::Vec3 p0 = Lerp(a, b, t0);
            Math::Vec3 p1 = Lerp(a, b, t1);
            m_lines.push_back({p0, shade(p0, base)});
            m_lines.push_back({p1, shade(p1, base)});
        }
    };

    for (i32 i = -cfg.halfExtent; i <= cfg.halfExtent; ++i) {
        f32 c = static_cast<f32>(i) * cfg.spacing;
        // Line parallel to Z at x = center.x + c (x = 0 line is the Z axis).
        emitLine({center.x + c, 0.0f, center.z - extent},
                 {center.x + c, 0.0f, center.z + extent},
                 lineColor(i, cfg.axisZ));
        // Line parallel to X at z = center.z + c (z = 0 line is the X axis).
        emitLine({center.x - extent, 0.0f, center.z + c},
                 {center.x + extent, 0.0f, center.z + c},
                 lineColor(i, cfg.axisX));
    }
}

}  // namespace Luma
