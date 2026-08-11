#pragma once

#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"
#include "Luma/RHI/Renderer.h"

// Luma::Grid - builds an "infinite" editor ground grid as world-space line
// vertices. The grid recenters on a focus point (the camera target) and its
// lines fade to the background color with distance, so it reads as infinite
// without any special shader. Pure geometry generation - the renderer stays
// backend-agnostic and knows nothing about grids.

namespace Luma {

struct GridConfig {
    i32 halfExtent = 90;  // lines span [-halfExtent, +halfExtent] cells
    f32 spacing = 1.0f;   // world units per cell
    i32 majorEvery = 10;  // every Nth line is a "major" line
    i32 segments = 48;    // subdivisions per line (for a smooth radial fade)

    // Each tier fades from its color to `fadeColor` between a start and end
    // radial distance. Minor lines fade out fast and near, so the distance
    // reads as a clean major grid (Unreal/Unity style) instead of dense fabric;
    // axes persist furthest.
    f32 minorFadeStart = 6.0f;
    f32 minorFadeEnd = 26.0f;
    f32 majorFadeStart = 24.0f;
    f32 majorFadeEnd = 82.0f;
    f32 axisFadeStart = 34.0f;
    f32 axisFadeEnd = 92.0f;

    Math::Vec3 minorColor{0.205f, 0.220f, 0.255f};
    Math::Vec3 majorColor{0.44f, 0.48f, 0.56f};
    Math::Vec3 axisX{0.88f, 0.26f, 0.30f};  // line along X (z = 0)
    Math::Vec3 axisZ{0.26f, 0.54f, 0.94f};  // line along Z (x = 0)
    Math::Vec3 fadeColor{0.10f, 0.11f, 0.13f};  // blends into the ground haze
};

class Grid {
public:
    // Rebuilds the grid centered on `focus` (snapped to the cell size). Cheap
    // enough to call per frame; no-op if the snapped center hasn't moved.
    void Build(const Math::Vec3& focus, const GridConfig& config = {});

    const std::vector<LineVertex>& Lines() const { return m_lines; }
    u32 VertexCount() const { return static_cast<u32>(m_lines.size()); }

private:
    std::vector<LineVertex> m_lines;
    Math::Vec3 m_center{1e9f, 0.0f, 1e9f};  // last snapped center (forces build)
    f32 m_spacing = -1.0f;                  // last cell size (forces build)
    bool m_built = false;
};

}  // namespace Luma
