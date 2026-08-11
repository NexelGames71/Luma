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

// Built-in mesh shapes the backend can draw. Real mesh assets replace this as
// the asset pipeline lands; feature modules generate the geometry (Luma::Mesh).
enum class MeshPrimitive { Cube, Plane, Sphere, Cylinder };

// One mesh instance: world transform, which primitive, and a PBR material.
struct SceneInstance {
    Math::Mat4 model = Math::Mat4::Identity();
    MeshPrimitive primitive = MeshPrimitive::Cube;
    Math::Vec3 albedo{0.82f, 0.82f, 0.85f};  // base color (linear)
    f32 metallic = 0.0f;
    f32 roughness = 0.5f;
};

// A punctual light fed to the scene shader. `type`: 0 = directional, 1 = point,
// 2 = spot. Directional ignores position/range; point ignores cone angles.
struct SceneLight {
    Math::Vec3 position{0.0f, 3.0f, 0.0f};
    u32 type = 1;
    Math::Vec3 direction{0.0f, -1.0f, 0.0f};  // spot/directional aim
    f32 range = 12.0f;
    Math::Vec3 color{1.0f, 1.0f, 1.0f};
    f32 intensity = 6.0f;
    f32 cosInner = 0.94f;  // spot cone (cos of inner half-angle)
    f32 cosOuter = 0.87f;  // spot cone (cos of outer half-angle)
};

// Analytic lighting + image-based-lighting environment, fed per frame. The sun
// comes from the Environment; the sky colors approximate the environment
// irradiance/reflection used for IBL (diffuse + specular ambient).
struct LightingParams {
    Math::Vec3 sunDirection{0.35f, 0.65f, 0.55f};  // world dir TO the sun
    Math::Vec3 sunColor{1.0f, 0.96f, 0.9f};
    f32 sunIntensity = 3.0f;
    Math::Vec3 skyZenith{0.20f, 0.34f, 0.62f};   // IBL: up
    Math::Vec3 skyHorizon{0.62f, 0.68f, 0.78f};  // IBL: horizon
    Math::Vec3 groundColor{0.16f, 0.16f, 0.17f};  // IBL: below horizon
    f32 iblIntensity = 1.0f;
};

// A world-space line-segment vertex (two per segment). Feature modules (grid,
// gizmo) produce these; the backend just renders line lists.
struct LineVertex {
    Math::Vec3 position;
    Math::Vec3 color;
};

// Procedural sky parameters. Fed by the Environment feature (an Environment
// entity's EnvironmentComponent); the backend renders an analytic Preetham sky
// as a fullscreen background when `enabled`. The renderer stays feature-agnostic
// — it consumes these numbers, it does not know about "environments".
struct SkyParams {
    Math::Vec3 sunDirection{0.35f, 0.65f, 0.55f};  // world dir TO the sun
    f32 turbidity = 2.6f;                          // atmospheric haze (1..10)
    Math::Vec3 groundColor{0.11f, 0.12f, 0.14f};   // color below the horizon
    f32 sunIntensity = 1.0f;                       // sun-disk brightness
    f32 skyIntensity = 1.0f;                       // overall sky exposure
    f32 sunSizeDegrees = 1.5f;                     // sun angular diameter
    bool enabled = false;
};

// Infinite editor ground grid, drawn by the backend as an analytic fullscreen
// ground-plane pass (ray-cast the Y=0 plane, screen-space derivative AA), so it
// is genuinely infinite. `fadeStart/End` are radial world distances the caller
// scales with the camera so the grid always fills the view at any zoom.
struct GridParams {
    Math::Vec3 minorColor{0.26f, 0.28f, 0.33f};
    Math::Vec3 majorColor{0.42f, 0.46f, 0.54f};
    Math::Vec3 axisX{0.88f, 0.26f, 0.30f};  // line where z = 0
    Math::Vec3 axisZ{0.26f, 0.54f, 0.94f};  // line where x = 0
    f32 cellSize = 1.0f;                     // minor cell world size
    i32 majorEvery = 10;                     // heavier line every N cells
    f32 fadeStart = 40.0f;
    f32 fadeEnd = 340.0f;
    bool enabled = false;
};

// Camera + everything to render for one viewport frame. The backend builds the
// projection from the fov and the target size, so callers only supply the view.
// Line data is produced by separate modules (e.g. Luma::Grid, Luma::Gizmo) and
// handed in here — the renderer stays feature-agnostic.
struct SceneView {
    SkyParams sky;
    GridParams grid;
    LightingParams lighting;

    Math::Mat4 view = Math::Mat4::Identity();
    f32 fovYRadians = 0.9f;
    f32 nearZ = 0.1f;
    f32 farZ = 500.0f;

    const SceneInstance* instances = nullptr;
    u32 instanceCount = 0;

    // Punctual lights (point/spot/directional), in addition to the Environment
    // sun that also drives the sky + IBL.
    const SceneLight* lights = nullptr;
    u32 lightCount = 0;

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
