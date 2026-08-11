#include "Luma/Grid/Grid.h"

namespace Luma {

void Grid::Build(const GridConfig& config) {
    m_lines.clear();
    const f32 extent = static_cast<f32>(config.halfExtent) * config.spacing;

    auto pick = [&](i32 i, bool axisLine, const Math::Vec3& axisColor) {
        if (i == 0) return axisColor;
        if (config.majorEvery > 0 && (i % config.majorEvery) == 0) {
            return config.majorColor;
        }
        (void)axisLine;
        return config.minorColor;
    };

    for (i32 i = -config.halfExtent; i <= config.halfExtent; ++i) {
        f32 c = static_cast<f32>(i) * config.spacing;

        // Line parallel to Z at x = c (the x = 0 line is the Z axis).
        Math::Vec3 zColor = pick(i, true, config.axisZ);
        m_lines.push_back({Math::Vec3(c, 0.0f, -extent), zColor});
        m_lines.push_back({Math::Vec3(c, 0.0f, extent), zColor});

        // Line parallel to X at z = c (the z = 0 line is the X axis).
        Math::Vec3 xColor = pick(i, true, config.axisX);
        m_lines.push_back({Math::Vec3(-extent, 0.0f, c), xColor});
        m_lines.push_back({Math::Vec3(extent, 0.0f, c), xColor});
    }
}

}  // namespace Luma
