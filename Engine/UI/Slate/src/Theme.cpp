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

    // Neutral surface ramp (deep window -> raised popup).
    t.surface0 = Color::RGB(16, 17, 21);
    t.surface1 = Color::RGB(24, 26, 32);
    t.surface2 = Color::RGB(33, 36, 44);
    t.surface3 = Color::RGB(45, 49, 59);
    t.surface4 = Color::RGB(52, 57, 68);

    // Refined Luma-blue accent ramp.
    t.accent = Color::RGB(64, 142, 240);
    t.accentHover = Color::RGB(88, 162, 255);
    t.accentActive = Color::RGB(46, 116, 208);
    t.accentMuted = Color::RGB(31, 60, 100);
    t.accentText = Color::RGB(247, 250, 255);

    // Semantic.
    t.separator = Color::RGB(11, 12, 15);
    t.outline = Color::RGB(58, 63, 74);
    t.focusRing = t.accent;
    t.text = Color::RGB(234, 237, 244);
    t.textDim = Color::RGB(150, 158, 172);
    t.textDisabled = Color::RGB(96, 102, 114);
    t.selectionBg = t.accentMuted;
    t.selectionText = t.accentText;
    t.success = Color::RGB(76, 180, 120);
    t.warning = Color::RGB(226, 170, 66);
    t.error = Color::RGB(226, 98, 98);

    // Fields.
    t.fieldBg = Color::RGB(19, 21, 26);
    t.fieldBorder = Color::RGB(56, 61, 72);

    // Legacy flat fields mapped onto the ramp (keeps existing widgets working).
    t.windowBg = t.surface0;
    t.panelBg = t.surface1;
    t.panelBorder = t.separator;
    t.header = t.surface2;
    t.button = t.surface3;
    t.buttonHover = t.surface4;
    t.buttonActive = Color::RGB(38, 41, 50);
    t.buttonText = Color::RGB(228, 231, 238);
    t.cardBg = t.surface2;
    t.cardHover = t.surface3;
    t.cardSelected = t.accentMuted;
    t.caret = t.text;
    t.rounding = t.radius.md;

    return t;
}

}  // namespace Luma::Slate
