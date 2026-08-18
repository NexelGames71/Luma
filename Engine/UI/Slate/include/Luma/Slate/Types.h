#pragma once

#include <cmath>

#include "Luma/Core/Types.h"

// Basic geometry + color primitives for Luma Slate, our own UI framework.

namespace Luma::Slate {

struct Vec2 {
    f32 x = 0.0f;
    f32 y = 0.0f;
};

struct Rect {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 w = 0.0f;
    f32 h = 0.0f;

    f32 Right() const { return x + w; }
    f32 Bottom() const { return y + h; }
    bool Contains(Vec2 p) const {
        return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
    }
    Rect Inset(f32 dx, f32 dy) const {
        return Rect{x + dx, y + dy, w - 2 * dx, h - 2 * dy};
    }
};

struct Color {
    u8 r = 0, g = 0, b = 0, a = 255;

    // Packed as 0xAABBGGRR to match RHI::UIVertex::color.
    u32 Packed() const {
        return u32(r) | (u32(g) << 8) | (u32(b) << 16) | (u32(a) << 24);
    }

    static Color RGB(u8 r, u8 g, u8 b) { return Color{r, g, b, 255}; }
    static Color RGBA(u8 r, u8 g, u8 b, u8 a) { return Color{r, g, b, a}; }
    Color WithAlpha(u8 alpha) const { return Color{r, g, b, alpha}; }
};

// --- RGB <-> HSV conversion -------------------------------------------------
// Hue is in degrees [0, 360); saturation and value in [0, 1]. These match the
// model used by the ColorPicker widget (and editors in general): hue drives the
// rainbow strip, S/V the gradient square.
inline void RGBToHSV(Color rgb, f32& h, f32& s, f32& v) {
    const f32 r = rgb.r / 255.0f;
    const f32 g = rgb.g / 255.0f;
    const f32 b = rgb.b / 255.0f;
    const f32 max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const f32 min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    const f32 d = max - min;

    v = max;
    s = (max > 0.0f) ? d / max : 0.0f;
    if (d < 1e-6f) {
        h = 0.0f;  // achromatic
        return;
    }

    f32 hue;
    if (max == r) {
        hue = 60.0f * ((g - b) / d);
    } else if (max == g) {
        hue = 60.0f * (2.0f + (b - r) / d);
    } else {
        hue = 60.0f * (4.0f + (r - g) / d);
    }
    if (hue < 0.0f) hue += 360.0f;
    h = hue;
}

inline Color HSVToRGB(f32 h, f32 s, f32 v) {
    // Wrap hue into [0, 360) and clamp S/V into [0, 1].
    while (h < 0.0f) h += 360.0f;
    while (h >= 360.0f) h -= 360.0f;
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    const f32 c = v * s;
    const f32 x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const f32 m = v - c;

    f32 r = 0.0f, g = 0.0f, b = 0.0f;
    if (h < 60.0f) {
        r = c; g = x;
    } else if (h < 120.0f) {
        r = x; g = c;
    } else if (h < 180.0f) {
        g = c; b = x;
    } else if (h < 240.0f) {
        g = x; b = c;
    } else if (h < 300.0f) {
        r = x; b = c;
    } else {
        r = c; b = x;
    }

    auto to8 = [](f32 t) -> u8 {
        int i = static_cast<int>(t * 255.0f + 0.5f);
        return static_cast<u8>(i < 0 ? 0 : (i > 255 ? 255 : i));
    };
    return Color::RGB(to8(r + m), to8(g + m), to8(b + m));
}

}  // namespace Luma::Slate
