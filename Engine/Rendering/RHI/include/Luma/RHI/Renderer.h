#pragma once

#include <string>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"

// Rendering Hardware Interface (RHI): the abstract surface the engine renders
// through. Concrete backends (VulkanRenderer today; D3D12/Metal later) implement
// this, keeping graphics-API types out of the rest of the engine.

namespace Luma {

struct ClearColor {
    f32 r = 0.0f;
    f32 g = 0.0f;
    f32 b = 0.0f;
    f32 a = 1.0f;
};

struct RendererConfig {
    std::string appName = "Luma";
    bool enableValidation = true;  // ignored in Shipping backends
    bool vsync = true;
};

// --- 2D UI drawing (Luma Slate produces this; the backend renders it) --------

// Opaque GPU texture handle. 0 is invalid.
using TextureHandle = u64;

// A UI vertex in screen-space pixels. `color` is packed RGBA8 (0xAABBGGRR).
struct UIVertex {
    f32 x, y;
    f32 u, v;
    u32 color;
};

// One draw call: a run of indices sampling `texture`, clipped to a rect (pixels).
struct UIDrawCommand {
    u32 indexOffset;
    u32 indexCount;
    TextureHandle texture;
    f32 clipX, clipY, clipW, clipH;
};

// A full UI frame's geometry. Pointers are owned by the caller and must remain
// valid for the duration of the DrawUI call.
struct UIDrawData {
    const UIVertex* vertices = nullptr;
    u32 vertexCount = 0;
    const u32* indices = nullptr;
    u32 indexCount = 0;
    const UIDrawCommand* commands = nullptr;
    u32 commandCount = 0;
    f32 displayWidth = 0.0f;
    f32 displayHeight = 0.0f;
};

// --- 3D scene view (rendered into an offscreen target for the viewport) -----

// One cube instance: its world transform and RGB color.
struct SceneInstance {
    Math::Mat4 model = Math::Mat4::Identity();
    Math::Vec3 color{0.8f, 0.8f, 0.85f};
};

// A world-space line-segment vertex (two per segment). Feature modules (grid,
// gizmo) produce these; the backend just renders line lists.
struct LineVertex {
    Math::Vec3 position;
    Math::Vec3 color;
};

// Camera + everything to render for one viewport frame. The backend builds the
// projection from the fov and the target size, so callers only supply the view.
// Line data is produced by separate modules (e.g. Luma::Grid, Luma::Gizmo) and
// handed in here — the renderer stays feature-agnostic.
struct SceneView {
    Math::Mat4 view = Math::Mat4::Identity();
    f32 fovYRadians = 0.9f;
    f32 nearZ = 0.1f;
    f32 farZ = 500.0f;

    const SceneInstance* instances = nullptr;
    u32 instanceCount = 0;

    // Depth-tested world lines (e.g. the ground grid).
    const LineVertex* lines = nullptr;
    u32 lineVertexCount = 0;

    // Lines drawn on top without depth testing (e.g. transform gizmos).
    const LineVertex* overlayLines = nullptr;
    u32 overlayLineVertexCount = 0;
};

class Renderer {
public:
    virtual ~Renderer() = default;

    // Notify the backend the drawable surface changed size (0x0 = minimized).
    virtual void OnResize(u32 width, u32 height) = 0;

    virtual void SetClearColor(const ClearColor& color) = 0;

    // Acquire the next frame. Returns false if the frame should be skipped
    // (e.g. the surface is out of date / minimized); do not call EndFrame then.
    virtual bool BeginFrame() = 0;

    // Submit and present the frame started by BeginFrame.
    virtual void EndFrame() = 0;

    // Records UI geometry into the current frame (call between Begin/EndFrame).
    virtual void DrawUI(const UIDrawData& data) = 0;

    // Creates an RGBA8 texture from tightly-packed pixels (width*height*4 bytes).
    virtual TextureHandle CreateTexture(u32 width, u32 height,
                                        const void* rgba8Pixels) = 0;
    virtual void DestroyTexture(TextureHandle texture) = 0;

    // Captures the frame currently being recorded to a PNG (call between
    // Begin/EndFrame). Only the rendered window contents are written.
    virtual void CaptureFrame(const std::string& pngPath) = 0;

    // Renders the given scene into an offscreen target and returns a UI texture
    // handle to display it. Call before BeginFrame.
    virtual TextureHandle RenderSceneView(u32 width, u32 height,
                                          const SceneView& scene) = 0;

    // Block until the device is idle (use before teardown).
    virtual void WaitIdle() = 0;
};

}  // namespace Luma
