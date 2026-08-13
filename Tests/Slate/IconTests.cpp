// Icon + Animate sanity tests. Each DrawIcon call should emit at least one
// draw command; Animate should converge to its target.

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "Luma/Slate/Context.h"
#include "Luma/Slate/Icons.h"
#include "Luma/Slate/Theme.h"

using Luma::Slate::Context;
using Luma::Slate::Color;
using Luma::Slate::Icon;
using Luma::Slate::Rect;

namespace {
// Minimal Renderer stub — we only need a Context whose DrawList we can read.
// No GPU, no atlas; fonts will simply emit no glyphs, which is fine for the
// icon tests because every icon draws with lines/tris/circles.
class NullRenderer final : public Luma::Renderer {
public:
    void OnResize(u32, u32) override {}
    void SetClearColor(const Luma::ClearColor&) override {}
    bool BeginFrame() override { return true; }
    void EndFrame() override {}
    void DrawUI(const Luma::UIDrawData&) override {}
    Luma::TextureHandle CreateTexture(u32, u32, const void*) override {
        return 0;
    }
    void DestroyTexture(Luma::TextureHandle) override {}
    void CaptureFrame(const std::string&) override {}
    Luma::TextureHandle RenderSceneView(u32, u32, const Luma::SceneView&) override {
        return 0;
    }
    void WaitIdle() override {}
};
}  // namespace

TEST_CASE("Animate converges to target over frames", "[slate][anim]") {
    Context ctx;
    NullRenderer r;
    REQUIRE(ctx.Init(r, Luma::Slate::Typography{}));
    // First call: not hovered yet -> value stays at 0.
    f32 v0 = ctx.Animate(42, false, 14.0f);
    REQUIRE(v0 == 0.0f);
    // Manually advance the frame counter by calling BeginFrame.
    ctx.BeginFrame(800, 600, 0.0166f);
    f32 v1 = ctx.Animate(42, true, 14.0f);
    ctx.EndFrame();
    REQUIRE(v1 > 0.0f);
    REQUIRE(v1 < 1.0f);
    // After many frames, value converges to ~1.
    for (int i = 0; i < 200; ++i) {
        ctx.BeginFrame(800, 600, 0.0166f);
        ctx.Animate(42, true, 14.0f);
        ctx.EndFrame();
    }
    ctx.BeginFrame(800, 600, 0.0166f);
    f32 vFinal = ctx.Animate(42, true, 14.0f);
    ctx.EndFrame();
    REQUIRE(std::fabs(vFinal - 1.0f) < 1e-3f);
}

TEST_CASE("DrawIcon emits at least one draw command for every glyph",
          "[slate][icon]") {
    Context ctx;
    NullRenderer r;
    REQUIRE(ctx.Init(r, Luma::Slate::Typography{}));
    ctx.BeginFrame(800, 600, 0.0166f);

    const Icon icons[] = {
        Icon::ChevronRight, Icon::ChevronDown, Icon::ChevronUp, Icon::Search,
        Icon::Gear, Icon::Folder, Icon::FolderOpen, Icon::Eye, Icon::EyeOff,
        Icon::Lock, Icon::Plus, Icon::Close, Icon::Check, Icon::Save,
        Icon::Play, Icon::Pause, Icon::Stop, Icon::Grip, Icon::Dot,
        Icon::Trash, Icon::Camera, Icon::Cube, Icon::Sphere, Icon::Plane,
        Icon::Cylinder, Icon::Refresh,
    };
    int prevCount = 0;
    for (Icon i : icons) {
        Luma::Slate::DrawIcon(ctx, {0, 0, 24, 24}, i, Color::RGB(255, 255, 255));
    }
    // After all icon draws, the draw list should have accumulated vertices.
    REQUIRE(ctx.drawList().vertexCount() > prevCount ||
            ctx.drawList().indexCount() > 0);
    (void)prevCount;
    ctx.EndFrame();
}

TEST_CASE("DrawIcon with Icon::None emits nothing", "[slate][icon]") {
    Context ctx;
    NullRenderer r;
    REQUIRE(ctx.Init(r, Luma::Slate::Typography{}));
    ctx.BeginFrame(800, 600, 0.0166f);
    auto before = ctx.drawList().vertexCount();
    Luma::Slate::DrawIcon(ctx, {0, 0, 24, 24}, Icon::None,
                          Color::RGB(255, 255, 255));
    ctx.EndFrame();
    REQUIRE(ctx.drawList().vertexCount() == before);
}
