#pragma once

#include <filesystem>
#include <functional>

#include "Luma/Math/Math.h"
#include "Luma/Scene/Scene.h"
#include "Luma/Slate/Context.h"

namespace Luma {
// Forward declarations keep PanelContext.h lightweight; only the panels that
// actually touch the gizmo need to include the full TranslateGizmo header.
class TranslateGizmo;
class Renderer;
class AssetRegistry;
}  // namespace Luma

namespace Luma::Editor::Panels {

using GizmoPtr = Luma::TranslateGizmo*;

// What the outliner's "Create GameObject" menu can spawn.
enum class CreateActorKind {
    Empty,
    Cube,
    Plane,
    Sphere,
    Cylinder,
    DirectionalLight,
    PointLight,
    SpotLight,
    TubeLight,
    Environment,
};

struct PanelContext {
    // Renderer for uploading dynamic textures (e.g. asset thumbnails).
    Renderer* renderer = nullptr;

    // Asset index (rooted at the project's Content/ folder). Panels that
    // pick assets (e.g. the Material Editor's TextureSample node) read the
    // texture list from this.
    AssetRegistry* assetRegistry = nullptr;

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
    std::function<void(CreateActorKind)> onCreateActor;  // Outliner's create menu
    std::function<void()> onSaveScene;     // File menu "Save Scene"

    // Content Browser right-click menu: creates a new .lmat material asset
    // in `folder` (empty = content root). Returns the created asset path on
    // success, or an empty path on failure.
    std::function<std::filesystem::path(const std::filesystem::path& folder)>
        onCreateMaterial;

    // Content Browser right-click menu "GET > Import to Current Folder":
    // opens a native file picker and imports the chosen files into `folder`
    // (empty = content root), then rescans the registry. Implemented by
    // EditorScreen; panels just invoke it.
    std::function<void(const std::filesystem::path& folder)> onImportAssets;
};

}  // namespace Luma::Editor::Panels
