// Theme invariants for the Slate design system.
//
// These tests don't render — they verify that the token bundle the rest of
// the UI reads from is internally consistent (ramp ordering, color helper
// correctness) so a future theme author can't silently break the hierarchy
// (e.g. a darker "raised" surface than the "panel" surface).

#include <catch2/catch_test_macros.hpp>

#include "Luma/Core/Types.h"
#include "Luma/Slate/Theme.h"

using namespace Luma;
using namespace Luma::Slate;

// Approximate luminance (Rec. 601) — good enough to compare ramp ordering
// without dragging in a full color science library.
static f32 ThemeLuma(const Color& c) {
    return 0.299f * static_cast<f32>(c.r) +
           0.587f * static_cast<f32>(c.g) +
           0.114f * static_cast<f32>(c.b);
}

TEST_CASE("DarkTheme surface ramp is monotonically lighter", "[slate][theme]") {
    auto t = DarkTheme();
    REQUIRE(ThemeLuma(t.surface0) < ThemeLuma(t.surface1));
    REQUIRE(ThemeLuma(t.surface1) < ThemeLuma(t.surface2));
    REQUIRE(ThemeLuma(t.surface2) < ThemeLuma(t.surface3));
    REQUIRE(ThemeLuma(t.surface3) < ThemeLuma(t.surface4));
}

TEST_CASE("DarkTheme accent ramp is correct ordering", "[slate][theme]") {
    auto t = DarkTheme();
    // accentActive < accent < accentHover (in luminance).
    REQUIRE(ThemeLuma(t.accentActive) < ThemeLuma(t.accent));
    REQUIRE(ThemeLuma(t.accent) < ThemeLuma(t.accentHover));
}

TEST_CASE("DarkTheme semantic colors are well-formed", "[slate][theme]") {
    auto t = DarkTheme();
    // Text colors should be brighter than the dim text, which is brighter
    // than disabled text.
    REQUIRE(ThemeLuma(t.text) > ThemeLuma(t.textDim));
    REQUIRE(ThemeLuma(t.textDim) > ThemeLuma(t.textDisabled));
    // Focus ring should equal the accent by convention.
    REQUIRE(t.focusRing.r == t.accent.r);
    REQUIRE(t.focusRing.g == t.accent.g);
    REQUIRE(t.focusRing.b == t.accent.b);
    // Selection text should be light enough to read on accentMuted.
    REQUIRE(ThemeLuma(t.selectionText) > ThemeLuma(t.selectionBg));
}

TEST_CASE("DarkTheme radius and border tokens are sane", "[slate][theme]") {
    auto t = DarkTheme();
    // none <= sm <= md <= lg, pill is a separate "fully rounded" bucket.
    REQUIRE(t.radius.none <= t.radius.sm);
    REQUIRE(t.radius.sm <= t.radius.md);
    REQUIRE(t.radius.md <= t.radius.lg);
    REQUIRE(t.radius.pill > t.radius.lg);
    // Border widths ordered.
    REQUIRE(t.border.hairline <= t.border.thick);
    // Motion factors are positive (otherwise the lerp degenerates).
    REQUIRE(t.motion.hover > 0.0f);
    REQUIRE(t.motion.press > 0.0f);
    REQUIRE(t.motion.popup > 0.0f);
}

TEST_CASE("Lighten at t=0 returns original color", "[slate][theme][helpers]") {
    Color c = Color::RGB(40, 60, 100);
    Color out = Lighten(c, 0.0f);
    REQUIRE(out.r == c.r);
    REQUIRE(out.g == c.g);
    REQUIRE(out.b == c.b);
    REQUIRE(out.a == c.a);
}

TEST_CASE("Lighten at t=1 returns white", "[slate][theme][helpers]") {
    Color c = Color::RGB(40, 60, 100);
    Color out = Lighten(c, 1.0f);
    REQUIRE(out.r == 255);
    REQUIRE(out.g == 255);
    REQUIRE(out.b == 255);
}

TEST_CASE("Darken at t=0 returns original color", "[slate][theme][helpers]") {
    Color c = Color::RGB(100, 150, 200);
    Color out = Darken(c, 0.0f);
    REQUIRE(out.r == c.r);
    REQUIRE(out.g == c.g);
    REQUIRE(out.b == c.b);
}

TEST_CASE("Darken at t=1 returns black", "[slate][theme][helpers]") {
    Color c = Color::RGB(100, 150, 200);
    Color out = Darken(c, 1.0f);
    REQUIRE(out.r == 0);
    REQUIRE(out.g == 0);
    REQUIRE(out.b == 0);
}

TEST_CASE("Mix endpoints return pure a and pure b", "[slate][theme][helpers]") {
    Color a = Color::RGB(20, 30, 40);
    Color b = Color::RGB(200, 180, 160);
    Color at0 = Mix(a, b, 0.0f);
    Color at1 = Mix(a, b, 1.0f);
    REQUIRE(at0.r == a.r);
    REQUIRE(at0.g == a.g);
    REQUIRE(at0.b == a.b);
    REQUIRE(at1.r == b.r);
    REQUIRE(at1.g == b.g);
    REQUIRE(at1.b == b.b);
}

TEST_CASE("Mix is order-independent at t=0.5 (rounded)", "[slate][theme][helpers]") {
    Color a = Color::RGB(10, 20, 30);
    Color b = Color::RGB(200, 150, 100);
    Color ab = Mix(a, b, 0.5f);
    Color ba = Mix(b, a, 0.5f);
    REQUIRE(ab.r == ba.r);
    REQUIRE(ab.g == ba.g);
    REQUIRE(ab.b == ba.b);
}
