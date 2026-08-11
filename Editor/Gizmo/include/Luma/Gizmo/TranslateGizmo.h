#pragma once

#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"
#include "Luma/RHI/Renderer.h"

// Luma::Gizmo - a 3D translate gizmo. This module owns only the gizmo's geometry
// and manipulation math; it produces overlay line vertices for the renderer and
// computes drag deltas. It knows nothing about Vulkan or the editor's data.

namespace Luma {

enum class GizmoAxis { None = -1, X = 0, Y = 1, Z = 2 };

// Screen-space info the gizmo needs to project itself and hit-test the mouse.
struct GizmoInput {
    Math::Mat4 view;
    Math::Mat4 proj;
    f32 viewportX = 0, viewportY = 0, viewportW = 0, viewportH = 0;
    f32 mouseX = 0, mouseY = 0;
    f32 scale = 1.0f;  // world length of the axes (matches BuildLines)
    bool leftDown = false;
    bool leftPressed = false;
};

class TranslateGizmo {
public:
    // Updates hover/drag against `position`. If the user is dragging an axis,
    // sets `outPosition` to the new position and returns true.
    bool Update(const Math::Vec3& position, const GizmoInput& in,
                Math::Vec3& outPosition);

    bool Dragging() const { return m_active != GizmoAxis::None; }

    // Rebuilds and returns overlay line geometry for the gizmo at `position`.
    // `scale` is the world-space length of the axes.
    const std::vector<LineVertex>& BuildLines(const Math::Vec3& position,
                                              f32 scale);

private:
    GizmoAxis m_hovered = GizmoAxis::None;
    GizmoAxis m_active = GizmoAxis::None;
    Math::Vec3 m_dragStartPos;
    f32 m_dragStartMouseX = 0, m_dragStartMouseY = 0;
    std::vector<LineVertex> m_lines;
};

}  // namespace Luma
