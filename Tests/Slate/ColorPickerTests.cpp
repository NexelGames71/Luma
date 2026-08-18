// ColorPicker widget tests. Verifies the RGB<->HSV conversion helpers and the
// picker's interactions: SV-square drag, hue-strip drag, spin-box commit on
// Enter, hex commit on Enter/blur, and OnColorChanged firing.
//
// Same frame/event pattern as WidgetTests.cpp: presses straddle a frame
// boundary so Slate's pressed/released tracking behaves like production.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "Luma/Core/Types.h"
#include "Luma/Slate/ColorPicker.h"
#include "Luma/Slate/ColorPickerPopup.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

using Luma::Slate::Color;
using Luma::Slate::ColorPicker;
using Luma::Slate::ColorPickerPopup;
using Luma::Slate::Context;
using Luma::Slate::Rect;
using Luma::f32;
using Luma::u8;

namespace {
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

Luma::Slate::Typography TestTypography() {
    Luma::Slate::Typography t;
    t.uiRegular = "C:/Windows/Fonts/segoeui.ttf";
    t.uiMedium = t.uiRegular;
    t.uiSemiBold = t.uiRegular;
    t.uiBold = t.uiRegular;
    t.mono = t.uiRegular;
    return t;
}

// Drawn at a fixed rect; the picker's SV square occupies
// {rect.x+8, rect.y+8, 128, 128} and the hue strip {rect.x+142, rect.y+8, 14, 128}.
const Rect kRect{0.0f, 0.0f, 260.0f, ColorPicker::kHeight};

void BeginFrame(Context& ctx) { ctx.BeginFrame(800, 600, 0.0166f); }
}  // namespace

TEST_CASE("RGBToHSV/HSVToRGB round-trip primary colors", "[slate][color]") {
    struct Case {
        Color rgb;
        f32 h;
        f32 s;
        f32 v;
    };
    const Case cases[] = {
        {Color::RGB(255, 0, 0), 0.0f, 1.0f, 1.0f},
        {Color::RGB(0, 255, 0), 120.0f, 1.0f, 1.0f},
        {Color::RGB(0, 0, 255), 240.0f, 1.0f, 1.0f},
        {Color::RGB(255, 255, 255), 0.0f, 0.0f, 1.0f},
        {Color::RGB(0, 0, 0), 0.0f, 0.0f, 0.0f},
        {Color::RGB(128, 128, 128), 0.0f, 0.0f, 128.0f / 255.0f},
    };
    for (const auto& c : cases) {
        f32 h = 0.0f, s = 0.0f, v = 0.0f;
        Luma::Slate::RGBToHSV(c.rgb, h, s, v);
        REQUIRE(h == Catch::Approx(c.h).margin(0.5f));
        REQUIRE(s == Catch::Approx(c.s).margin(0.001f));
        REQUIRE(v == Catch::Approx(c.v).margin(0.001f));
        // HSV -> RGB round-trip must return the exact source color.
        Color back = Luma::Slate::HSVToRGB(h, s, v);
        REQUIRE(back.r == c.rgb.r);
        REQUIRE(back.g == c.rgb.g);
        REQUIRE(back.b == c.rgb.b);
    }
}

TEST_CASE("HSVToRGB wraps hue and clamps S/V", "[slate][color]") {
    // -120 == 240 (blue); oversaturated clamps to 1.
    Color c = Luma::Slate::HSVToRGB(-120.0f, 2.0f, 1.0f);
    REQUIRE(c.b == 255);
    REQUIRE(c.r == 0);
    REQUIRE(c.g == 0);
    // 480 == 120 (green).
    c = Luma::Slate::HSVToRGB(480.0f, 1.0f, 1.0f);
    REQUIRE(c.g == 255);
    // Negative value clamps to 0 -> black.
    c = Luma::Slate::HSVToRGB(0.0f, 1.0f, -1.0f);
    REQUIRE(c.r == 0);
    REQUIRE(c.g == 0);
    REQUIRE(c.b == 0);
}

TEST_CASE("ColorPicker draws without crashing and keeps the initial color",
          "[slate][colorpicker]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    ColorPicker picker(Color::RGB(10, 200, 30));
    BeginFrame(ctx);
    bool changed = picker.Draw(ctx, kRect);
    ctx.EndFrame();
    REQUIRE_FALSE(changed);
    REQUIRE(picker.color().r == 10);
    REQUIRE(picker.color().g == 200);
    REQUIRE(picker.color().b == 30);
}

TEST_CASE("ColorPicker SV drag updates color and fires OnColorChanged",
          "[slate][colorpicker]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    ColorPicker picker(Color::RGB(255, 0, 0));  // h=0, s=1, v=1
    int fires = 0;
    picker.OnColorChanged = [&](const Color&) { ++fires; };

    // Press in the middle of the SV square (s=0.5, v=0.5) and drag to the
    // top-left corner (s=0, v=1) -> white.
    BeginFrame(ctx);
    ctx.OnMouseMove(8.0f + 64.0f, 8.0f + 64.0f);
    ctx.OnMouseButton(0, true);
    picker.Draw(ctx, kRect);
    ctx.EndFrame();

    BeginFrame(ctx);
    ctx.OnMouseMove(8.0f, 8.0f);
    picker.Draw(ctx, kRect);
    ctx.EndFrame();

    // Release.
    BeginFrame(ctx);
    ctx.OnMouseButton(0, false);
    picker.Draw(ctx, kRect);
    ctx.EndFrame();

    REQUIRE(fires >= 1);
    REQUIRE(picker.color().r == 255);
    REQUIRE(picker.color().g == 255);
    REQUIRE(picker.color().b == 255);
}

TEST_CASE("ColorPicker hue strip drag changes hue, preserving S/V",
          "[slate][colorpicker]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    ColorPicker picker(Color::RGB(255, 0, 0));  // h=0
    // Hue strip: {142, 8, 14, 128}. Drag to the vertical middle -> h=180
    // (cyan at s=1, v=1).
    BeginFrame(ctx);
    ctx.OnMouseMove(150.0f, 8.0f);
    ctx.OnMouseButton(0, true);
    picker.Draw(ctx, kRect);
    ctx.EndFrame();

    BeginFrame(ctx);
    ctx.OnMouseMove(150.0f, 8.0f + 64.0f);
    picker.Draw(ctx, kRect);
    ctx.EndFrame();

    BeginFrame(ctx);
    ctx.OnMouseButton(0, false);
    picker.Draw(ctx, kRect);
    ctx.EndFrame();

    REQUIRE(picker.color().g == 255);
    REQUIRE(picker.color().b == 255);
    REQUIRE(picker.color().r == 0);
}

TEST_CASE("ColorPicker hex field commits valid input on Enter",
          "[slate][colorpicker]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    ColorPicker picker(Color::RGB(255, 0, 0));
    int fires = 0;
    picker.OnColorChanged = [&](const Color&) { ++fires; };

    // Hex field: right column swatch {x..} — swatch rect is
    // {x+8+128+6+14+6+6, 8, w, 40} and the hex field sits 6px below it.
    // Click the hex field, type "00FF00", press Enter.
    BeginFrame(ctx);
    ctx.OnMouseMove(190.0f, 8.0f + 40.0f + 6.0f + 12.0f);
    ctx.OnMouseButton(0, true);
    picker.Draw(ctx, kRect);
    ctx.EndFrame();

    BeginFrame(ctx);
    ctx.OnMouseButton(0, false);
    ctx.OnText('0');
    ctx.OnText('0');
    ctx.OnText('F');
    ctx.OnText('F');
    ctx.OnText('0');
    ctx.OnText('0');
    picker.Draw(ctx, kRect);
    ctx.EndFrame();

    BeginFrame(ctx);
    ctx.OnKey(/*GLFW_KEY_ENTER=*/257, true);
    picker.Draw(ctx, kRect);
    ctx.EndFrame();

    REQUIRE(fires >= 1);
    REQUIRE(picker.color().r == 0);
    REQUIRE(picker.color().g == 255);
    REQUIRE(picker.color().b == 0);
}

TEST_CASE("ColorPicker hex field reverts invalid input on blur",
          "[slate][colorpicker]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    ColorPicker picker(Color::RGB(255, 0, 0));

    // Enter "ZZZ" into the hex field, then click the preview swatch (which
    // takes no input, so it's a pure blur) -> invalid hex reverts.
    BeginFrame(ctx);
    ctx.OnMouseMove(190.0f, 8.0f + 40.0f + 6.0f + 12.0f);
    ctx.OnMouseButton(0, true);
    picker.Draw(ctx, kRect);
    ctx.EndFrame();

    BeginFrame(ctx);
    ctx.OnMouseButton(0, false);
    ctx.OnText('Z');
    ctx.OnText('Z');
    ctx.OnText('Z');
    picker.Draw(ctx, kRect);
    ctx.EndFrame();

    // Click the preview swatch (outside the hex field) -> invalid hex reverts
    // and the color stays red.
    BeginFrame(ctx);
    ctx.OnMouseMove(200.0f, 20.0f);
    ctx.OnMouseButton(0, true);
    picker.Draw(ctx, kRect);
    ctx.EndFrame();

    REQUIRE(picker.color().r == 255);
    REQUIRE(picker.color().g == 0);
    REQUIRE(picker.color().b == 0);
}

TEST_CASE("ColorPickerPopup opens on swatch click and hosts the picker",
          "[slate][colorpicker]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    ColorPickerPopup popup(Color::RGB(0, 0, 255));
    Rect swatch{10.0f, 10.0f, 40.0f, 24.0f};

    // Click the swatch -> popup opens.
    BeginFrame(ctx);
    ctx.OnMouseMove(30.0f, 22.0f);
    ctx.OnMouseButton(0, true);
    popup.Draw(ctx, swatch);
    ctx.EndFrame();
    BeginFrame(ctx);
    ctx.OnMouseButton(0, false);
    popup.Draw(ctx, swatch);
    ctx.EndFrame();
    REQUIRE(popup.IsOpen());

    // Outside-click closes it.
    BeginFrame(ctx);
    ctx.OnMouseMove(700.0f, 500.0f);
    ctx.OnMouseButton(0, true);
    popup.Draw(ctx, swatch);
    ctx.EndFrame();
    REQUIRE_FALSE(popup.IsOpen());
}

TEST_CASE("ColorPickerPopup split DrawSwatch/DrawPanel renders the panel",
          "[slate][colorpicker]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    ColorPickerPopup popup(Color::RGB(0, 0, 255));
    Rect swatch{10.0f, 10.0f, 40.0f, 24.0f};

    // Click the swatch (DrawSwatch) -> opens.
    BeginFrame(ctx);
    ctx.OnMouseMove(30.0f, 22.0f);
    ctx.OnMouseButton(0, true);
    popup.DrawSwatch(ctx, swatch);
    ctx.EndFrame();
    BeginFrame(ctx);
    ctx.OnMouseButton(0, false);
    popup.DrawSwatch(ctx, swatch);
    ctx.EndFrame();
    REQUIRE(popup.IsOpen());

    // DrawPanel emits the floating picker geometry and doesn't change color.
    BeginFrame(ctx);
    auto before = ctx.drawList().vertexCount();
    bool changed = popup.DrawPanel(ctx);
    ctx.EndFrame();
    REQUIRE(ctx.drawList().vertexCount() > before);
    REQUIRE_FALSE(changed);
    REQUIRE(popup.color().r == 0);
    REQUIRE(popup.color().b == 255);
}

TEST_CASE("ColorPicker SetColor re-syncs the picker externally",
          "[slate][colorpicker]") {
    Context ctx;
    NullRenderer r;
    ctx.Init(r, TestTypography());
    ColorPicker picker(Color::RGB(255, 0, 0));
    picker.SetColor(Color::RGB(0, 128, 255));
    BeginFrame(ctx);
    picker.Draw(ctx, kRect);
    ctx.EndFrame();
    REQUIRE(picker.color().r == 0);
    REQUIRE(picker.color().g == 128);
    REQUIRE(picker.color().b == 255);
}
