#pragma once

#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"
#include "Luma/RHI/Renderer.h"

// Luma::Grid - builds the editor ground grid as a list of world-space line
// vertices. This module owns only geometry generation; the renderer draws the
// lines it produces (kept fully backend-agnostic).

namespace Luma {

struct GridConfig {
    i32 halfExtent = 20;    // lines span [-halfExtent, +halfExtent] cells
    f32 spacing = 1.0f;     // world units per cell
    i32 majorEvery = 10;    // every Nth line is a "major" line
    Math::Vec3 minorColor{0.26f, 0.28f, 0.32f};
    Math::Vec3 majorColor{0.40f, 0.43f, 0.49f};
    Math::Vec3 axisX{0.80f, 0.30f, 0.33f};  // line along +X (z = 0)
    Math::Vec3 axisZ{0.33f, 0.48f, 0.82f};  // line along +Z (x = 0)
};

class Grid {
public:
    // Rebuilds the line list for the given configuration.
    void Build(const GridConfig& config = {});

    const std::vector<LineVertex>& Lines() const { return m_lines; }
    u32 VertexCount() const { return static_cast<u32>(m_lines.size()); }

private:
    std::vector<LineVertex> m_lines;
};

}  // namespace Luma
