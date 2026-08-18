#include "Luma/Slate/Theme.h"

#include <algorithm>

namespace Luma::Slate {
namespace {
u8 ClampU8(f32 v) {
    return static_cast<u8>(std::clamp(v, 0.0f, 255.0f) + 0.5f);
}
}  // namespace

Color Lighten(Color c, f32 amount) {
    return Color{ClampU8(c.r + (255.0f - c.r) * amount),
                 ClampU8(c.g + (255.0f - c.g) * amount),
                 ClampU8(c.b + (255.0f - c.b) * amount), c.a};
}

Color Darken(Color c, f32 amount) {
    f32 k = 1.0f - amount;
    return Color{ClampU8(c.r * k), ClampU8(c.g * k), ClampU8(c.b * k), c.a};
}

Color Mix(Color a, Color b, f32 t) {
    return Color{ClampU8(a.r + (b.r - a.r) * t), ClampU8(a.g + (b.g - a.g) * t),
                 ClampU8(a.b + (b.b - a.b) * t), ClampU8(a.a + (b.a - a.a) * t)};
}

Theme DarkTheme() {
    Theme t;

    // Unreal-style neutral surface ramp: deep window -> raised popup.
    // Colors roughly mirror Unreal's editor palette so panels feel familiar.
    t.surface0 = Color::RGB(30, 30, 30);   // window background
    t.surface1 = Color::RGB(37, 37, 38);   // toolbars / breadcrumbs
    t.surface2 = Color::RGB(45, 45, 48);   // panel bg, card bg
    t.surface3 = Color::RGB(63, 63, 70);   // raised (button resting)
    t.surface4 = Color::RGB(80, 80, 86);   // popups / headers

    // Unreal-style orange accent ramp.
    t.accent = Color::RGB(255, 140, 0);
    t.accentHover = Color::RGB(255, 170, 50);
    t.accentActive = Color::RGB(220, 110, 0);
    t.accentMuted = Color::RGB(100, 55, 0);
    t.accentText = Color::RGB(255, 230, 200);

    // Semantic.
    t.separator = Color::RGB(20, 20, 20);
    t.outline = Color::RGB(58, 58, 60);
    t.focusRing = t.accent;
    t.text = Color::RGB(234, 237, 244);
    t.textDim = Color::RGB(180, 180, 184);
    t.textDisabled = Color::RGB(110, 110, 116);
    t.selectionBg = Color::RGB(38, 79, 138);
    t.selectionText = Color::RGB(255, 255, 255);
    t.success = Color::RGB(76, 180, 120);
    t.warning = Color::RGB(226, 170, 66);
    t.error = Color::RGB(226, 98, 98);

    // Fields (search box, text fields).
    t.fieldBg = Color::RGB(28, 28, 30);
    t.fieldBorder = Color::RGB(60, 60, 64);

    // Legacy flat fields mapped onto the ramp (keeps existing widgets working).
    t.windowBg = t.surface0;
    t.panelBg = t.surface1;
    t.panelBorder = t.separator;
    t.header = t.surface2;
    t.button = t.surface3;
    t.buttonHover = t.surface4;
    t.buttonActive = Color::RGB(50, 50, 54);
    t.buttonText = Color::RGB(228, 231, 238);
    t.cardBg = t.surface2;
    t.cardHover = t.surface3;
    t.cardSelected = t.accentMuted;
    t.caret = t.text;
    t.rounding = t.radius.md;

    // Menu rows sit on the surface4 popup; hover = surface3 (darker, visible
    // against the raised menu bg), focus = one step lighter than hover so the
    // keyboard-focus row reads as a distinct state (plus the orange accent
    // bar the caller draws).
    t.menu.hoverFill = t.surface3;
    t.menu.focusFill = Lighten(t.surface3, 0.08f);
    // Unreal-style row highlight: vivid Luma-orange fill with white text
    // (Unreal uses its blue; we keep the orange identity).
    t.menu.highlightFill = Color::RGB(186, 103, 12);
    t.menu.highlightText = Color::RGB(255, 255, 255);

    return t;
}

}  // namespace Luma::Slate
