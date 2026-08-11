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
    i32 halfExtent = 50;   // lines span [-halfExtent, +halfExtent] cells
    f32 spacing = 1.0f;    // world units per cell
    i32 majorEvery = 10;   // every Nth line is a "major" line
    i32 segments = 40;     // subdivisions per line (for a smooth radial fade)
    f32 fadeStart = 14.0f; // world distance where fading begins
    f32 fadeEnd = 46.0f;   // world distance where lines reach the fade color
    Math::Vec3 minorColor{0.30f, 0.32f, 0.37f};
    Math::Vec3 majorColor{0.44f, 0.47f, 0.53f};
    Math::Vec3 axisX{0.82f, 0.30f, 0.33f};  // line along X (z = 0)
    Math::Vec3 axisZ{0.33f, 0.50f, 0.85f};  // line along Z (x = 0)
    Math::Vec3 fadeColor{0.07f, 0.08f, 0.10f};  // viewport background
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
    bool m_built = false;
};

}  // namespace Luma
