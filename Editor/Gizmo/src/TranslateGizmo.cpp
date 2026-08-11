#include "Luma/Gizmo/TranslateGizmo.h"

#include <cmath>

namespace Luma {
namespace {

// Projects a world point to viewport pixel coordinates. Returns false if the
// point is behind the camera.
bool WorldToScreen(const Math::Vec3& p, const GizmoInput& in, f32& outX,
                   f32& outY) {
    using namespace Math;
    // clip = proj * view * (p, 1)
    Mat4 vp = in.proj * in.view;
    f32 x = p.x, y = p.y, z = p.z;
    f32 cx = vp.m[0] * x + vp.m[4] * y + vp.m[8] * z + vp.m[12];
    f32 cy = vp.m[1] * x + vp.m[5] * y + vp.m[9] * z + vp.m[13];
    f32 cw = vp.m[3] * x + vp.m[7] * y + vp.m[11] * z + vp.m[15];
    if (cw <= 0.0001f) return false;
    f32 ndcX = cx / cw;
    f32 ndcY = cy / cw;  // proj already flips Y for the framebuffer
    outX = in.viewportX + (ndcX * 0.5f + 0.5f) * in.viewportW;
    outY = in.viewportY + (ndcY * 0.5f + 0.5f) * in.viewportH;
    return true;
}

Math::Vec3 AxisDir(GizmoAxis a) {
    switch (a) {
        case GizmoAxis::X: return {1, 0, 0};
        case GizmoAxis::Y: return {0, 1, 0};
        case GizmoAxis::Z: return {0, 0, 1};
        default: return {0, 0, 0};
    }
}

// Distance from point p to segment ab, in 2D.
f32 DistToSegment(f32 px, f32 py, f32 ax, f32 ay, f32 bx, f32 by) {
    f32 dx = bx - ax, dy = by - ay;
    f32 len2 = dx * dx + dy * dy;
    f32 t = len2 > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0.0f;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    f32 cx = ax + dx * t, cy = ay + dy * t;
    f32 ex = px - cx, ey = py - cy;
    return std::sqrt(ex * ex + ey * ey);
}

}  // namespace

bool TranslateGizmo::Update(const Math::Vec3& position, const GizmoInput& in,
                            Math::Vec3& outPosition) {
    using namespace Math;
    // Axis length must match what BuildLines draws so hit-testing lines up.
    const f32 axisLen = in.scale > 0.0f ? in.scale : 1.0f;

    // Continue an active drag.
    if (m_active != GizmoAxis::None) {
        if (!in.leftDown) {
            m_active = GizmoAxis::None;
            return false;
        }
        Vec3 axis = AxisDir(m_active);
        f32 ox, oy, tx, ty;
        bool okO = WorldToScreen(m_dragStartPos, in, ox, oy);
        bool okT = WorldToScreen(m_dragStartPos + axis * axisLen, in, tx, ty);
        if (okO && okT) {
            f32 sx = tx - ox, sy = ty - oy;
            f32 sLen2 = sx * sx + sy * sy;
            if (sLen2 > 0.0001f) {
                f32 mvx = in.mouseX - m_dragStartMouseX;
                f32 mvy = in.mouseY - m_dragStartMouseY;
                f32 along = (mvx * sx + mvy * sy) / sLen2;  // in axis-length units
                outPosition = m_dragStartPos + axis * (along * axisLen);
                return true;
            }
        }
        return false;
    }

    // Hover detection: nearest axis segment within a pixel threshold.
    f32 ox, oy;
    if (!WorldToScreen(position, in, ox, oy)) {
        m_hovered = GizmoAxis::None;
        return false;
    }
    m_hovered = GizmoAxis::None;
    f32 best = 10.0f;  // pixel threshold
    for (int a = 0; a < 3; ++a) {
        Vec3 axis = AxisDir(static_cast<GizmoAxis>(a));
        f32 tx, ty;
        if (!WorldToScreen(position + axis * axisLen, in, tx, ty)) continue;
        f32 d = DistToSegment(in.mouseX, in.mouseY, ox, oy, tx, ty);
        if (d < best) {
            best = d;
            m_hovered = static_cast<GizmoAxis>(a);
        }
    }

    if (m_hovered != GizmoAxis::None && in.leftPressed) {
        m_active = m_hovered;
        m_dragStartPos = position;
        m_dragStartMouseX = in.mouseX;
        m_dragStartMouseY = in.mouseY;
    }
    return false;
}

const std::vector<LineVertex>& TranslateGizmo::BuildLines(
    const Math::Vec3& position, f32 scale) {
    using namespace Math;
    m_lines.clear();
    const Vec3 baseColors[3] = {
        {0.90f, 0.25f, 0.28f}, {0.35f, 0.85f, 0.35f}, {0.30f, 0.55f, 0.95f}};
    for (int a = 0; a < 3; ++a) {
        Vec3 axis = AxisDir(static_cast<GizmoAxis>(a));
        Vec3 color = baseColors[a];
        GizmoAxis ga = static_cast<GizmoAxis>(a);
        if (ga == m_active || (m_active == GizmoAxis::None && ga == m_hovered)) {
            color = Vec3(1.0f, 0.95f, 0.4f);  // highlight
        }
        m_lines.push_back({position, color});
        m_lines.push_back({position + axis * scale, color});
    }
    return m_lines;
}

}  // namespace Luma
