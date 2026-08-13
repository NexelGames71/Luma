#pragma once

#include <functional>

#include "Luma/Math/Math.h"
#include "Luma/Scene/Scene.h"
#include "Luma/Slate/Context.h"

namespace Luma {
// Forward declarations keep PanelContext.h lightweight; only the panels that
// actually touch the gizmo need to include the full TranslateGizmo header.
class TranslateGizmo;
}  // namespace Luma

namespace Luma::Editor::Panels {

using GizmoPtr = Luma::TranslateGizmo*;

struct PanelContext {
    // Scene + ECS the panels drive.
    Scene* scene = nullptr;
    Entity* selected = nullptr;        // outliner sets, inspector + gizmo read

    // Orbit camera (viewport + inspector preview).
    f32* camYaw = nullptr;
    f32* camPitch = nullptr;
    f32* camDistance = nullptr;
    Math::Vec3* camTarget = nullptr;
    f32 fovY = 0.9f;
    f32 nearZ = 0.1f;
    f32 farZ = 500.0f;
    Math::Mat4* view = nullptr;        // rebuilt each BuildSceneView
    f32* gizmoScale = nullptr;

    // Gizmo (viewport hosts it; selection feeds it).
    GizmoPtr gizmo = nullptr;

    // EditorScreen hooks (lightweight callbacks so panels don't have to
    // know about the EditorScreen type).
    std::function<void()> onAddEntity;     // Outliner's "+ Add"
    std::function<void()> onSaveScene;     // File menu "Save Scene"
};

}  // namespace Luma::Editor::Panels
