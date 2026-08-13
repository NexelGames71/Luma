#include "Luma/Slate/Icons.h"

#include <cmath>

namespace Luma::Slate {
namespace {

// Pixel size for a 1-unit stroke, multiplied by dpiScale so icons stay crisp
// on hi-DPI displays without per-icon DPI bookkeeping.
f32 StrokeWidth(const Context& ctx) {
    return 1.5f * ctx.dpiScale();
}

void DrawChevronRight(Context& ctx, const Rect& box, Color color, bool open) {
    f32 cx = box.x + box.w * 0.45f;
    f32 cy = box.y + box.h * 0.5f;
    f32 s = std::min(box.w, box.h) * 0.22f;
    if (open) {
        // Down-pointing chevron
        ctx.drawList().AddLine({cx - s, cy - s * 0.4f}, {cx, cy + s * 0.6f}, color,
                           StrokeWidth(ctx));
        ctx.drawList().AddLine({cx, cy + s * 0.6f}, {cx + s, cy - s * 0.4f}, color,
                           StrokeWidth(ctx));
    } else {
        // Right-pointing chevron
        ctx.drawList().AddLine({cx - s * 0.4f, cy - s}, {cx + s * 0.6f, cy}, color,
                           StrokeWidth(ctx));
        ctx.drawList().AddLine({cx + s * 0.6f, cy}, {cx - s * 0.4f, cy + s}, color,
                           StrokeWidth(ctx));
    }
}

void DrawSearch(Context& ctx, const Rect& box, Color color) {
    Vec2 c{box.x + box.w * 0.4f, box.y + box.h * 0.4f};
    f32 r = std::min(box.w, box.h) * 0.22f;
    f32 inner = r - StrokeWidth(ctx);
    if (inner < 1.0f) inner = r * 0.6f;
    // Ring via two concentric circles (outline-like)
    ctx.drawList().AddCircleFilled(c, r, color.WithAlpha(40));
    ctx.drawList().AddCircleFilled(c, inner, Color::RGBA(0, 0, 0, 0));
    // Handle
    f32 hx0 = c.x + r * 0.7f;
    f32 hy0 = c.y + r * 0.7f;
    f32 hx1 = box.Right() - box.w * 0.12f;
    f32 hy1 = box.Bottom() - box.h * 0.12f;
    ctx.drawList().AddLine({hx0, hy0}, {hx1, hy1}, color, StrokeWidth(ctx));
}

void DrawGear(Context& ctx, const Rect& box, Color color) {
    Vec2 c{box.x + box.w * 0.5f, box.y + box.h * 0.5f};
    f32 ro = std::min(box.w, box.h) * 0.36f;
    f32 ri = ro * 0.6f;
    ctx.drawList().AddCircleFilled(c, ro, color);
    // 8 teeth
    for (int i = 0; i < 8; ++i) {
        f32 a = (static_cast<f32>(i) / 8.0f) * 6.2831853f;
        Vec2 p{c.x + std::cos(a) * ro, c.y + std::sin(a) * ro};
        f32 tooth = ro * 0.18f;
        Vec2 q{c.x + std::cos(a) * (ro + tooth),
               c.y + std::sin(a) * (ro + tooth)};
        ctx.drawList().AddLine(p, q, color, StrokeWidth(ctx) * 1.5f);
    }
    ctx.drawList().AddCircleFilled(c, ri, ctx.theme().surface2);
}

void DrawFolder(Context& ctx, const Rect& box, Color color, bool open) {
    f32 x = box.x + box.w * 0.08f;
    f32 y = box.y + box.h * 0.30f;
    f32 w = box.w * 0.84f;
    f32 h = box.h * 0.58f;
    // Tab
    f32 tabW = w * 0.4f;
    f32 tabH = h * 0.18f;
    Vec2 pts[7] = {
        {x, y + tabH},               {x + tabW * 0.6f, y + tabH},
        {x + tabW, y},               {x + w, y},
        {x + w, y + h},              {x, y + h},
        {x, y + tabH},
    };
    ctx.drawList().AddConvexPolyFilled(pts, 7, color);
    if (open) {
        // Slight wedge at the top to read as "open"
        Vec2 cut{x + w * 0.55f, y};
        Vec2 mid{x + w * 0.65f, y + tabH * 0.4f};
        (void)cut;
        (void)mid;
    }
}

void DrawEye(Context& ctx, const Rect& box, Color color, bool off) {
    Vec2 c{box.x + box.w * 0.5f, box.y + box.h * 0.5f};
    f32 rx = box.w * 0.36f;
    f32 ry = box.h * 0.26f;
    // Outline-ish lens: two ovals approximated as circles since we're low-fi.
    ctx.drawList().AddCircleFilled(c, ry, color.WithAlpha(60));
    if (!off) {
        ctx.drawList().AddCircleFilled(c, ry * 0.55f, color);
    }
    if (off) {
        ctx.drawList().AddLine({box.x + box.w * 0.12f, box.Bottom() - box.h * 0.12f},
                           {box.Right() - box.w * 0.12f,
                            box.y + box.h * 0.12f},
                           color, StrokeWidth(ctx) * 1.5f);
        (void)rx;
    }
}

void DrawLock(Context& ctx, const Rect& box, Color color) {
    f32 bw = box.w * 0.62f;
    f32 bh = box.h * 0.42f;
    f32 bx = box.x + (box.w - bw) * 0.5f;
    f32 by = box.y + box.h * 0.42f;
    ctx.drawList().AddRectFilled({bx, by, bw, bh}, color);
    // Shackle: half-circle outline
    Vec2 c{bx + bw * 0.5f, by};
    f32 r = bw * 0.28f;
    ctx.drawList().AddCircleFilled({c.x, c.y - r}, r, color.WithAlpha(60));
    ctx.drawList().AddCircleFilled({c.x, c.y - r}, r * 0.5f,
                               Color::RGBA(0, 0, 0, 0));
}

void DrawPlus(Context& ctx, const Rect& box, Color color) {
    f32 cx = box.x + box.w * 0.5f;
    f32 cy = box.y + box.h * 0.5f;
    f32 s = box.w * 0.32f;
    ctx.drawList().AddLine({cx - s, cy}, {cx + s, cy}, color, StrokeWidth(ctx) * 1.5f);
    ctx.drawList().AddLine({cx, cy - s}, {cx, cy + s}, color, StrokeWidth(ctx) * 1.5f);
}

void DrawClose(Context& ctx, const Rect& box, Color color) {
    f32 s = box.w * 0.28f;
    Vec2 a0{box.x + box.w * 0.5f - s, box.y + box.h * 0.5f - s};
    Vec2 a1{box.x + box.w * 0.5f + s, box.y + box.h * 0.5f + s};
    Vec2 b0{box.x + box.w * 0.5f + s, box.y + box.h * 0.5f - s};
    Vec2 b1{box.x + box.w * 0.5f - s, box.y + box.h * 0.5f + s};
    ctx.drawList().AddLine(a0, a1, color, StrokeWidth(ctx) * 1.5f);
    ctx.drawList().AddLine(b0, b1, color, StrokeWidth(ctx) * 1.5f);
}

void DrawCheck(Context& ctx, const Rect& box, Color color) {
    f32 s = box.w * 0.3f;
    Vec2 c{box.x + box.w * 0.5f, box.y + box.h * 0.5f};
    ctx.drawList().AddLine({c.x - s, c.y}, {c.x - s * 0.3f, c.y + s * 0.7f},
                       color, StrokeWidth(ctx) * 1.5f);
    ctx.drawList().AddLine({c.x - s * 0.3f, c.y + s * 0.7f},
                       {c.x + s, c.y - s * 0.6f},
                       color, StrokeWidth(ctx) * 1.5f);
}

void DrawSave(Context& ctx, const Rect& box, Color color) {
    f32 x = box.x + box.w * 0.18f;
    f32 y = box.y + box.h * 0.18f;
    f32 w = box.w * 0.64f;
    f32 h = box.h * 0.64f;
    ctx.drawList().AddRectFilled({x, y, w, h}, color);
    ctx.drawList().AddRectFilled({x + w * 0.1f, y, w * 0.8f, h * 0.28f},
                             ctx.theme().surface2);
    ctx.drawList().AddRectFilled({x + w * 0.3f, y + h * 0.45f, w * 0.4f, h * 0.42f},
                             ctx.theme().surface2);
}

void DrawTriangleGlyph(Context& ctx, const Rect& box, Color color,
                       bool pointRight) {
    Vec2 c{box.x + box.w * 0.5f, box.y + box.h * 0.5f};
    f32 s = box.w * 0.32f;
    if (pointRight) {
        ctx.drawList().AddTriangle({c.x - s, c.y - s}, {c.x - s, c.y + s},
                               {c.x + s, c.y}, color);
    } else {
        ctx.drawList().AddTriangle({c.x - s, c.y - s}, {c.x - s, c.y + s},
                               {c.x + s, c.y}, color);
        (void)0;
    }
}

void DrawStopGlyph(Context& ctx, const Rect& box, Color color) {
    f32 s = box.w * 0.26f;
    ctx.drawList().AddRectFilled({box.x + box.w * 0.5f - s, box.y + box.h * 0.5f - s,
                              s * 2.0f, s * 2.0f},
                             color);
}

void DrawPause(Context& ctx, const Rect& box, Color color) {
    f32 s = box.w * 0.16f;
    f32 h = box.h * 0.5f;
    f32 cx = box.x + box.w * 0.5f;
    f32 cy = box.y + box.h * 0.5f;
    ctx.drawList().AddRectFilled({cx - s - 2.0f, cy - h * 0.5f, s, h}, color);
    ctx.drawList().AddRectFilled({cx + 2.0f, cy - h * 0.5f, s, h}, color);
}

void DrawGrip(Context& ctx, const Rect& box, Color color) {
    f32 cx = box.x + box.w * 0.5f;
    f32 cy = box.y + box.h * 0.5f;
    f32 gap = box.w * 0.12f;
    f32 r = box.w * 0.06f;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            ctx.drawList().AddCircleFilled({cx + dx * gap, cy + dy * gap}, r,
                                       color);
        }
    }
}

void DrawDot(Context& ctx, const Rect& box, Color color) {
    f32 r = std::min(box.w, box.h) * 0.22f;
    ctx.drawList().AddCircleFilled({box.x + box.w * 0.5f, box.y + box.h * 0.5f},
                               r, color);
}

void DrawTrash(Context& ctx, const Rect& box, Color color) {
    f32 x = box.x + box.w * 0.18f;
    f32 y = box.y + box.h * 0.28f;
    f32 w = box.w * 0.64f;
    f32 h = box.h * 0.58f;
    // Lid
    f32 lidH = box.h * 0.1f;
    ctx.drawList().AddRectFilled({box.x + box.w * 0.10f, y, w + box.w * 0.12f, lidH},
                             color);
    // Body
    ctx.drawList().AddRectFilled({x, y + lidH, w, h - lidH}, color);
    // Handle
    f32 hh = box.h * 0.16f;
    ctx.drawList().AddRectFilled({box.x + box.w * 0.40f, y - hh, box.w * 0.20f, hh},
                             color);
    // Slats
    f32 sx = x + w * 0.18f;
    f32 sy = y + lidH + box.h * 0.1f;
    f32 sw = box.w * 0.06f;
    f32 sh = h - lidH - box.h * 0.18f;
    for (int i = 0; i < 3; ++i) {
        ctx.drawList().AddRectFilled({sx + i * (sw + box.w * 0.10f), sy, sw, sh},
                                 ctx.theme().surface2);
    }
}

void DrawPrimitiveCube(Context& ctx, const Rect& box, Color color) {
    f32 x = box.x + box.w * 0.2f;
    f32 y = box.y + box.h * 0.2f;
    f32 s = box.w * 0.6f;
    ctx.drawList().AddRectFilled({x, y, s, s}, color);
    ctx.drawList().AddTriangle({x, y}, {x + s * 0.3f, y - s * 0.3f},
                           {x + s, y}, color.WithAlpha(180));
    ctx.drawList().AddTriangle({x + s, y}, {x + s + s * 0.3f, y - s * 0.3f},
                           {x + s + s * 0.3f, y + s * 0.7f}, color.WithAlpha(140));
    ctx.drawList().AddTriangle({x + s, y}, {x + s + s * 0.3f, y + s * 0.7f},
                           {x + s, y + s}, color.WithAlpha(120));
}

void DrawPrimitiveSphere(Context& ctx, const Rect& box, Color color) {
    Vec2 c{box.x + box.w * 0.5f, box.y + box.h * 0.5f};
    f32 r = box.w * 0.32f;
    ctx.drawList().AddCircleFilled(c, r, color);
    ctx.drawList().AddCircleFilled({c.x - r * 0.3f, c.y - r * 0.3f}, r * 0.18f,
                               color.WithAlpha(220));
}

void DrawPrimitivePlane(Context& ctx, const Rect& box, Color color) {
    Vec2 pts[4] = {
        {box.x + box.w * 0.1f, box.y + box.h * 0.7f},
        {box.x + box.w * 0.9f, box.y + box.h * 0.7f},
        {box.x + box.w * 0.75f, box.y + box.h * 0.85f},
        {box.x + box.w * 0.25f, box.y + box.h * 0.85f}};
    ctx.drawList().AddConvexPolyFilled(pts, 4, color);
}

void DrawPrimitiveCylinder(Context& ctx, const Rect& box, Color color) {
    f32 x = box.x + box.w * 0.3f;
    f32 y = box.y + box.h * 0.15f;
    f32 w = box.w * 0.4f;
    f32 h = box.h * 0.7f;
    ctx.drawList().AddRectFilled({x, y, w, h}, color);
    ctx.drawList().AddCircleFilled({x + w * 0.5f, y}, w * 0.5f, color.WithAlpha(200));
    ctx.drawList().AddCircleFilled({x + w * 0.5f, y + h}, w * 0.5f, color.WithAlpha(140));
}

void DrawRefresh(Context& ctx, const Rect& box, Color color) {
    f32 cx = box.x + box.w * 0.5f;
    f32 cy = box.y + box.h * 0.5f;
    f32 r = box.w * 0.32f;
    ctx.drawList().AddCircleFilled({cx, cy}, r, color.WithAlpha(40));
    // Arrow head
    Vec2 p0{cx + r * 0.9f, cy - r * 0.2f};
    Vec2 p1{cx + r * 0.6f, cy - r * 0.7f};
    Vec2 p2{cx + r * 1.05f, cy - r * 0.55f};
    ctx.drawList().AddTriangle(p0, p1, p2, color);
}

}  // namespace

void DrawIcon(Context& ctx, const Rect& box, Icon icon, Color color) {
    switch (icon) {
        case Icon::ChevronRight: DrawChevronRight(ctx, box, color, false); break;
        case Icon::ChevronDown:  DrawChevronRight(ctx, box, color, true);  break;
        case Icon::ChevronUp:    DrawChevronRight(ctx, box, color, true);
            (void)0; break;
        case Icon::Search:       DrawSearch(ctx, box, color); break;
        case Icon::Gear:         DrawGear(ctx, box, color); break;
        case Icon::Folder:       DrawFolder(ctx, box, color, false); break;
        case Icon::FolderOpen:   DrawFolder(ctx, box, color, true); break;
        case Icon::Eye:          DrawEye(ctx, box, color, false); break;
        case Icon::EyeOff:       DrawEye(ctx, box, color, true); break;
        case Icon::Lock:         DrawLock(ctx, box, color); break;
        case Icon::Plus:         DrawPlus(ctx, box, color); break;
        case Icon::Close:        DrawClose(ctx, box, color); break;
        case Icon::Check:        DrawCheck(ctx, box, color); break;
        case Icon::Save:         DrawSave(ctx, box, color); break;
        case Icon::Play:         DrawTriangleGlyph(ctx, box, color, true); break;
        case Icon::Pause:        DrawPause(ctx, box, color); break;
        case Icon::Stop:         DrawStopGlyph(ctx, box, color); break;
        case Icon::Grip:         DrawGrip(ctx, box, color); break;
        case Icon::Dot:          DrawDot(ctx, box, color); break;
        case Icon::Trash:        DrawTrash(ctx, box, color); break;
        case Icon::Camera:       DrawPrimitiveCube(ctx, box, color); break;
        case Icon::Light:        DrawDot(ctx, box, color); break;
        case Icon::Cube:         DrawPrimitiveCube(ctx, box, color); break;
        case Icon::Sphere:       DrawPrimitiveSphere(ctx, box, color); break;
        case Icon::Plane:        DrawPrimitivePlane(ctx, box, color); break;
        case Icon::Cylinder:     DrawPrimitiveCylinder(ctx, box, color); break;
        case Icon::Refresh:      DrawRefresh(ctx, box, color); break;
        case Icon::None:         break;
    }
}

}  // namespace Luma::Slate
