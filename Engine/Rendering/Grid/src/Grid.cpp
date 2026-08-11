#include "Luma/Grid/Grid.h"

#include <cmath>
#include <utility>

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
    // Rebuild when the snapped center OR the cell size changes (the editor
    // scales spacing with the camera for an infinite look). Cheap per frame.
    if (m_built && center.x == m_center.x && center.z == m_center.z &&
        cfg.spacing == m_spacing) {
        return;
    }
    m_center = center;
    m_spacing = cfg.spacing;
    m_built = true;

    m_lines.clear();
    const f32 extent = static_cast<f32>(cfg.halfExtent) * cfg.spacing;
    const int segs = cfg.segments > 0 ? cfg.segments : 1;

    // A line tier: its base color and the radial fade window it uses.
    struct Tier {
        Math::Vec3 color;
        f32 fadeStart;
        f32 fadeEnd;
    };
    enum Kind { Minor, Major, Axis };
    auto tierOf = [&](i32 i, bool axisIsX) -> std::pair<Kind, Tier> {
        if (i == 0) {
            return {Axis, {axisIsX ? cfg.axisX : cfg.axisZ, cfg.axisFadeStart,
                           cfg.axisFadeEnd}};
        }
        if (cfg.majorEvery > 0 && (i % cfg.majorEvery) == 0) {
            return {Major, {cfg.majorColor, cfg.majorFadeStart, cfg.majorFadeEnd}};
        }
        return {Minor, {cfg.minorColor, cfg.minorFadeStart, cfg.minorFadeEnd}};
    };

    // Colors a point by its planar distance from the grid center (fade -> bg).
    auto shade = [&](const Math::Vec3& p, const Tier& tier) {
        f32 dx = p.x - center.x;
        f32 dz = p.z - center.z;
        f32 d = std::sqrt(dx * dx + dz * dz);
        f32 f = SmoothStep(tier.fadeStart, tier.fadeEnd, d);
        return Lerp(tier.color, cfg.fadeColor, f);
    };

    // Emits one grid line (tessellated) so the color can fade along its length.
    auto emitLine = [&](const Math::Vec3& a, const Math::Vec3& b,
                        const Tier& tier) {
        for (int s = 0; s < segs; ++s) {
            f32 t0 = static_cast<f32>(s) / segs;
            f32 t1 = static_cast<f32>(s + 1) / segs;
            Math::Vec3 p0 = Lerp(a, b, t0);
            Math::Vec3 p1 = Lerp(a, b, t1);
            m_lines.push_back({p0, shade(p0, tier)});
            m_lines.push_back({p1, shade(p1, tier)});
        }
    };

    for (i32 i = -cfg.halfExtent; i <= cfg.halfExtent; ++i) {
        f32 c = static_cast<f32>(i) * cfg.spacing;
        f32 dist = std::abs(c);

        // Line parallel to Z at x = center.x + c (x = 0 line is the Z axis).
        auto [zKind, zTier] = tierOf(i, /*axisIsX=*/false);
        if (!(zKind == Minor && dist > cfg.minorFadeEnd)) {
            emitLine({center.x + c, 0.0f, center.z - extent},
                     {center.x + c, 0.0f, center.z + extent}, zTier);
        }
        // Line parallel to X at z = center.z + c (z = 0 line is the X axis).
        auto [xKind, xTier] = tierOf(i, /*axisIsX=*/true);
        if (!(xKind == Minor && dist > cfg.minorFadeEnd)) {
            emitLine({center.x - extent, 0.0f, center.z + c},
                     {center.x + extent, 0.0f, center.z + c}, xTier);
        }
    }
}

}  // namespace Luma
