#pragma once

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

}  // namespace Luma::Slate
