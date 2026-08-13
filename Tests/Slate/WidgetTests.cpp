// Widget catalog tests. Drives Context with a NullRenderer stub and feeds
// synthetic mouse + key events to verify state transitions for the new
// widgets: Toggle, Slider, Dropdown, TreeNode, SearchBox, MenuPopup,
// Tooltip, ProgressBar, PropertyRow.
//
// Pattern: each "click" test is two frames. Frame 1: move + press.
// Frame 2: release + draw widget. Slate tracks `pressed`/`released` between
// BeginFrame/EndFrame boundaries, so the press and release must straddle a
// frame boundary.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Icons.h"
#include "Luma/Slate/Theme.h"

using Luma::Slate::Context;
using Luma::Slate::Color;
using Luma::Slate::Icon;
using Luma::Slate::Rect;
using Luma::f32;

namespace {
// Minimal Renderer stub — fonts bake no glyphs (Init returns false), but
// widget state transitions don't depend on rendering.
class NullRenderer final : public Luma::Renderer {
public:
    void OnResize(Luma::u32, Luma::u32) override {}
    void SetClearColor(const Luma::ClearColor&) override {}
    bool BeginFrame() override { return true; }
    void EndFrame() override {}
    void DrawUI(const Luma::UIDrawData&) override {}
    Luma::TextureHandle CreateTexture(Luma::u32, Luma::u32,
                                      const void*) override {
        return 0;
    }
    void DestroyTexture(Luma::TextureHandle) override {}
    void CaptureFrame(const std::string&) override {}
    Luma::TextureHandle RenderSceneView(Luma::u32, Luma::u32,
                                        const Luma::SceneView&) override {
        return 0;
    }
    void WaitIdle() override {}
};

// A typography that uses the system Segoe UI font so widget text rendering
// can actually run (otherwise font atlases don't bake, but no widget logic
// path crashes — except `m_draw.AddText` exits early, so we use this so
// every widget path is exercised as in production).
Luma::Slate::Typography TestTypography() {
    Luma::Slate::Typography t;
    t.uiRegular = "C:/Windows/Fonts/segoeui.ttf";
    t.uiMedium = t.uiRegular;
    t.uiSemiBold = t.uiRegular;
    t.uiBold = t.uiRegular;
    t.mono = t.uiRegular;
    return t;
}

// Two-frame click pattern mirroring production. Press in frame A so the
// widget draws with m_mousePressed[0] set and stores m_active = id. Release
// in frame B so the next draw sees m_mouseReleased[0] + persisted m_active
// and fires the click. The lambda runs once per frame.
template <typename Fn>
void Click(Context& ctx, f32 x, f32 y, int button, Fn&& widget) {
    ctx.BeginFrame(800, 600, 0.0166f);
    ctx.OnMouseMove(x, y);
    ctx.OnMouseButton(button, true);
    widget();
    ctx.EndFrame();
    ctx.BeginFrame(800, 600, 0.0166f);
    ctx.OnMouseButton(button, false);
    widget();
    ctx.EndFrame();
}
}  // namespace

TEST_CASE("Toggle flips value on click inside its rect", "[slate][widget]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    bool v = false;
    Click(ctx, 50.0f, 10.0f, 0, [&] {
        ctx.Toggle(42, {40, 0, 60, 20}, v);
    });
    REQUIRE(v == true);
}

TEST_CASE("Toggle does nothing on click outside its rect",
          "[slate][widget]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    bool v = false;
    Click(ctx, 500.0f, 500.0f, 0, [&] {
        ctx.Toggle(42, {40, 0, 60, 20}, v);
    });
    REQUIRE(v == false);
}

TEST_CASE("Slider clamps to [min, max] on click inside", "[slate][widget]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    f32 v = 0.0f;
    Click(ctx, 190.0f, 10.0f, 0, [&] {
        ctx.Slider(1, {0, 0, 200, 20}, v, 0.0f, 10.0f);
    });
    REQUIRE(v > 9.0f);
    REQUIRE(v <= 10.0f);
}

TEST_CASE("Dropdown opens on field click; selection returns new index",
          "[slate][widget]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    std::vector<std::string> items{"A", "B", "C"};
    int idx = 0;
    // First click: open the dropdown.
    Click(ctx, 50.0f, 10.0f, 0, [&] {
        idx = ctx.Dropdown(7, {0, 0, 100, 24}, items, 0);
    });
    REQUIRE(idx == 0);  // opening does not change selection
    // Second click: select item B (y ~ 60 inside the open list).
    Click(ctx, 50.0f, 60.0f, 0, [&] {
        idx = ctx.Dropdown(7, {0, 0, 100, 24}, items, 0);
    });
    REQUIRE(idx == 1);
}

TEST_CASE("TreeNode toggles open state on click", "[slate][widget]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    bool open = false;
    Click(ctx, 50.0f, 10.0f, 0, [&] {
        ctx.TreeNode(3, {0, 0, 200, 24}, "Root", Icon::Folder, open, 0,
                     false);
    });
    REQUIRE(open);
}

TEST_CASE("SearchBox edits text via typing and clears via X",
          "[slate][widget]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    std::string text;
    // Type "hi" and render in the same frame so m_textInput applies before
    // EndFrame clears it.
    ctx.BeginFrame(800, 600, 0.0166f);
    ctx.OnMouseMove(40.0f, 10.0f);
    ctx.OnMouseButton(0, true);
    ctx.OnText('h');
    ctx.OnText('i');
    ctx.SearchBox(1, {0, 0, 200, 24}, text);
    ctx.OnMouseButton(0, false);
    ctx.EndFrame();
    REQUIRE(text == "hi");
    // Click X (rightmost icon).
    Click(ctx, 195.0f, 10.0f, 0, [&] {
        ctx.SearchBox(1, {0, 0, 200, 24}, text);
    });
    REQUIRE(text.empty());
}

TEST_CASE("PropertyRow returns a non-empty field rect", "[slate][widget]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    ctx.BeginFrame(800, 600, 0.0166f);
    Rect field = ctx.PropertyRow({0, 0, 300, 24}, "Position");
    ctx.EndFrame();
    REQUIRE(field.w > 0.0f);
    REQUIRE(field.h > 0.0f);
}

TEST_CASE("ProgressBar emits non-zero draws for a partial fraction",
          "[slate][widget]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    ctx.BeginFrame(800, 600, 0.0166f);
    auto before = ctx.drawList().vertexCount();
    ctx.ProgressBar({0, 0, 200, 16}, 0.5f);
    ctx.EndFrame();
    REQUIRE(ctx.drawList().vertexCount() > before);
}

TEST_CASE("Tooltip shows only after kTooltipDelay frames of hover",
          "[slate][widget]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    // Hover, fewer frames than the delay -> no tooltip draw yet.
    ctx.BeginFrame(800, 600, 0.0166f);
    ctx.OnMouseMove(50.0f, 10.0f);
    ctx.EndFrame();
    auto before = ctx.drawList().vertexCount();
    for (int i = 0; i < 5; ++i) {
        ctx.BeginFrame(800, 600, 0.0166f);
        ctx.OnMouseMove(50.0f, 10.0f);
        ctx.Tooltip({0, 0, 100, 24}, "Hello");
        ctx.EndFrame();
    }
    auto afterShort = ctx.drawList().vertexCount();
    REQUIRE(afterShort == before);
    // Exceed the delay.
    for (int i = 0; i < 40; ++i) {
        ctx.BeginFrame(800, 600, 0.0166f);
        ctx.OnMouseMove(50.0f, 10.0f);
        ctx.Tooltip({0, 0, 100, 24}, "Hello");
        ctx.EndFrame();
    }
    REQUIRE(ctx.drawList().vertexCount() > afterShort);
}

TEST_CASE("BeginModal returns content rect; EndModal clears state",
          "[slate][widget]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    ctx.BeginFrame(800, 600, 0.0166f);
    Rect content = ctx.BeginModal(99, "Test", {320, 200});
    REQUIRE(content.w > 0.0f);
    REQUIRE(content.h > 0.0f);
    ctx.EndModal();
    ctx.EndFrame();
}
