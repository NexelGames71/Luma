#include "Luma/Slate/DrawList.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Luma::Slate {
namespace {
constexpr f32 kPi = 3.14159265358979323846f;
}  // namespace

void DrawList::Begin(f32 displayWidth, f32 displayHeight) {
    m_vertices.clear();
    m_indices.clear();
    m_commands.clear();
    m_clipStack.clear();
    m_displayWidth = displayWidth;
    m_displayHeight = displayHeight;
}

Rect DrawList::CurrentClip() const {
    if (!m_clipStack.empty()) return m_clipStack.back();
    return Rect{0.0f, 0.0f, m_displayWidth, m_displayHeight};
}

void DrawList::PushClip(Rect clip) {
    // Nested clips intersect with the enclosing clip so a child region can
    // never extend past its parent (which would let content escape a panel).
    if (!m_clipStack.empty()) {
        const Rect& parent = m_clipStack.back();
        f32 x0 = std::max(clip.x, parent.x);
        f32 y0 = std::max(clip.y, parent.y);
        f32 x1 = std::min(clip.Right(), parent.Right());
        f32 y1 = std::min(clip.Bottom(), parent.Bottom());
        clip = Rect{x0, y0, std::max(0.0f, x1 - x0),
                    std::max(0.0f, y1 - y0)};
    }
    m_clipStack.push_back(clip);
}
void DrawList::PopClip() {
    if (!m_clipStack.empty()) m_clipStack.pop_back();
}

void DrawList::EnsureCommand(TextureHandle texture) {
    Rect clip = CurrentClip();
    bool needNew = m_commands.empty();
    if (!needNew) {
        const UIDrawCommand& last = m_commands.back();
        needNew = last.texture != texture || last.clipX != clip.x ||
                  last.clipY != clip.y || last.clipW != clip.w ||
                  last.clipH != clip.h;
    }
    if (needNew) {
        UIDrawCommand cmd{};
        cmd.indexOffset = static_cast<u32>(m_indices.size());
        cmd.indexCount = 0;
        cmd.texture = texture;
        cmd.clipX = clip.x;
        cmd.clipY = clip.y;
        cmd.clipW = clip.w;
        cmd.clipH = clip.h;
        m_commands.push_back(cmd);
    }
}

void DrawList::AddQuad(TextureHandle texture, const Rect& dst, const Rect& uv,
                       Color color) {
    EnsureCommand(texture);
    u32 base = static_cast<u32>(m_vertices.size());
    u32 packed = color.Packed();
    m_vertices.push_back({dst.x, dst.y, uv.x, uv.y, packed});
    m_vertices.push_back({dst.Right(), dst.y, uv.Right(), uv.y, packed});
    m_vertices.push_back({dst.Right(), dst.Bottom(), uv.Right(), uv.Bottom(),
                          packed});
    m_vertices.push_back({dst.x, dst.Bottom(), uv.x, uv.Bottom(), packed});

    u32 quad[6] = {base, base + 1, base + 2, base + 2, base + 3, base};
    for (u32 i : quad) m_indices.push_back(i);
    m_commands.back().indexCount += 6;
}

void DrawList::AddRectFilled(const Rect& rect, Color color) {
    // texture 0 = the backend's 1x1 white texture.
    AddQuad(0, rect, Rect{0.0f, 0.0f, 1.0f, 1.0f}, color);
}

void DrawList::AddConvexPolyFilled(const Vec2* points, int count, Color color) {
    if (count < 3) return;
    EnsureCommand(0);
    u32 base = static_cast<u32>(m_vertices.size());
    u32 packed = color.Packed();
    for (int i = 0; i < count; ++i) {
        m_vertices.push_back({points[i].x, points[i].y, 0.5f, 0.5f, packed});
    }
    for (int i = 1; i < count - 1; ++i) {
        m_indices.push_back(base);
        m_indices.push_back(base + static_cast<u32>(i));
        m_indices.push_back(base + static_cast<u32>(i + 1));
    }
    m_commands.back().indexCount += static_cast<u32>((count - 2) * 3);
}

void DrawList::AddRectFilledRounded(const Rect& rect, Color color,
                                    f32 radius) {
    f32 r = std::min(radius, std::min(rect.w, rect.h) * 0.5f);
    if (r <= 0.75f) {
        AddRectFilled(rect, color);
        return;
    }
    constexpr int kSeg = 5;  // points per corner arc
    std::vector<Vec2> pts;
    pts.reserve((kSeg + 1) * 4);
    Vec2 tl{rect.x + r, rect.y + r};
    Vec2 tr{rect.Right() - r, rect.y + r};
    Vec2 br{rect.Right() - r, rect.Bottom() - r};
    Vec2 bl{rect.x + r, rect.Bottom() - r};
    auto arc = [&](Vec2 c, f32 a0, f32 a1) {
        for (int i = 0; i <= kSeg; ++i) {
            f32 a = a0 + (a1 - a0) * (static_cast<f32>(i) / kSeg);
            pts.push_back({c.x + std::cos(a) * r, c.y + std::sin(a) * r});
        }
    };
    arc(tl, kPi, kPi * 1.5f);
    arc(tr, kPi * 1.5f, kPi * 2.0f);
    arc(br, 0.0f, kPi * 0.5f);
    arc(bl, kPi * 0.5f, kPi);
    AddConvexPolyFilled(pts.data(), static_cast<int>(pts.size()), color);
}

void DrawList::AddRectFilledGradient(const Rect& rect, Color top,
                                     Color bottom) {
    EnsureCommand(0);
    u32 base = static_cast<u32>(m_vertices.size());
    u32 t = top.Packed();
    u32 b = bottom.Packed();
    m_vertices.push_back({rect.x, rect.y, 0.0f, 0.0f, t});
    m_vertices.push_back({rect.Right(), rect.y, 1.0f, 0.0f, t});
    m_vertices.push_back({rect.Right(), rect.Bottom(), 1.0f, 1.0f, b});
    m_vertices.push_back({rect.x, rect.Bottom(), 0.0f, 1.0f, b});
    u32 quad[6] = {base, base + 1, base + 2, base + 2, base + 3, base};
    for (u32 i : quad) m_indices.push_back(i);
    m_commands.back().indexCount += 6;
}

void DrawList::AddRectFilledGradientH(const Rect& rect, Color left,
                                      Color right) {
    EnsureCommand(0);
    u32 base = static_cast<u32>(m_vertices.size());
    u32 l = left.Packed();
    u32 r = right.Packed();
    m_vertices.push_back({rect.x, rect.y, 0.0f, 0.0f, l});
    m_vertices.push_back({rect.Right(), rect.y, 1.0f, 0.0f, r});
    m_vertices.push_back({rect.Right(), rect.Bottom(), 1.0f, 1.0f, r});
    m_vertices.push_back({rect.x, rect.Bottom(), 0.0f, 1.0f, l});
    u32 quad[6] = {base, base + 1, base + 2, base + 2, base + 3, base};
    for (u32 i : quad) m_indices.push_back(i);
    m_commands.back().indexCount += 6;
}

void DrawList::AddTriangle(Vec2 a, Vec2 b, Vec2 c, Color color) {
    EnsureCommand(0);
    u32 base = static_cast<u32>(m_vertices.size());
    u32 packed = color.Packed();
    m_vertices.push_back({a.x, a.y, 0.0f, 0.0f, packed});
    m_vertices.push_back({b.x, b.y, 0.5f, 1.0f, packed});
    m_vertices.push_back({c.x, c.y, 1.0f, 0.0f, packed});
    m_indices.push_back(base);
    m_indices.push_back(base + 1);
    m_indices.push_back(base + 2);
    m_commands.back().indexCount += 3;
}

void DrawList::AddRectOutline(const Rect& rect, Color color, f32 t) {
    AddRectFilled(Rect{rect.x, rect.y, rect.w, t}, color);                  // top
    AddRectFilled(Rect{rect.x, rect.Bottom() - t, rect.w, t}, color);       // bottom
    AddRectFilled(Rect{rect.x, rect.y, t, rect.h}, color);                  // left
    AddRectFilled(Rect{rect.Right() - t, rect.y, t, rect.h}, color);        // right
}

void DrawList::AddRectOutlineRounded(const Rect& rect, Color color,
                                     f32 t, f32 radius) {
    if (t <= 0.0f) return;
    f32 r = std::min(radius, std::min(rect.w, rect.h) * 0.5f);
    if (r <= 0.75f) {
        AddRectOutline(rect, color, t);
        return;
    }
    f32 halfT = t * 0.5f;
    // Outer + inner corner centers share the same pivot; the stroke is the
    // band between radius r-t/2 and r+t/2 (clamped).
    f32 ro = r + halfT;
    f32 ri = std::max(0.0f, r - halfT);
    // Straight edge bars span between the corner arcs (lengthwise ±r from
    // each side) and are `t` thick, centered on the perimeter.
    // Top edge.
    AddRectFilled({rect.x + r, rect.y - halfT, rect.w - 2.0f * r, t}, color);
    // Bottom edge.
    AddRectFilled({rect.x + r, rect.Bottom() - halfT, rect.w - 2.0f * r, t},
                  color);
    // Left edge.
    AddRectFilled({rect.x - halfT, rect.y + r, t, rect.h - 2.0f * r}, color);
    // Right edge.
    AddRectFilled({rect.Right() - halfT, rect.y + r, t, rect.h - 2.0f * r},
                  color);
    // Corner arcs: each arc is sampled as N quads between radius ri and ro
    // around its corner pivot. The four corners use pivots:
    Vec2 pivots[4] = {
        {rect.x + r, rect.y + r},               // top-left  (sweep pi..1.5pi)
        {rect.Right() - r, rect.y + r},          // top-right (sweep 1.5pi..2pi)
        {rect.Right() - r, rect.Bottom() - r},  // br        (sweep 0..0.5pi)
        {rect.x + r, rect.Bottom() - r},        // bl        (sweep 0.5pi..pi)
    };
    f32 aStart[4] = {kPi, kPi * 1.5f, 0.0f, kPi * 0.5f};
    for (int c = 0; c < 4; ++c) {
        constexpr int kSeg = 5;
        Vec2 p = pivots[c];
        for (int i = 0; i < kSeg; ++i) {
            f32 a0 = aStart[c] + (kPi * 0.5f) * (static_cast<f32>(i) / kSeg);
            f32 a1 = aStart[c] + (kPi * 0.5f) *
                                     (static_cast<f32>(i + 1) / kSeg);
            Vec2 o0o{p.x + std::cos(a0) * ro, p.y + std::sin(a0) * ro};
            Vec2 o1o{p.x + std::cos(a1) * ro, p.y + std::sin(a1) * ro};
            Vec2 o0i{p.x + std::cos(a0) * ri, p.y + std::sin(a0) * ri};
            Vec2 o1i{p.x + std::cos(a1) * ri, p.y + std::sin(a1) * ri};
            Vec2 quad[4] = {o0o, o1o, o1i, o0i};
            AddConvexPolyFilled(quad, 4, color);
        }
    }
}

void DrawList::AddLine(Vec2 a, Vec2 b, Color color, f32 thickness) {
    // Axis-aligned (or near-axis) lines collapse to rects. Diagonal lines are
    // rasterized as a thin rotated quad. For our purposes (icons, splitters,
    // gizmo strokes) most lines are short and not pixel-perfect — a quad is
    // fine.
    Vec2 delta{b.x - a.x, b.y - a.y};
    f32 len = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (len < 1e-3f || thickness <= 0.0f) {
        // Degenerate: draw a 1px dot.
        AddRectFilled({a.x, a.y, 1.0f, 1.0f}, color);
        return;
    }
    if (std::fabs(delta.y) < 1e-4f) {
        AddRectFilled({std::min(a.x, b.x), a.y - thickness * 0.5f,
                       len, thickness},
                      color);
        return;
    }
    if (std::fabs(delta.x) < 1e-4f) {
        AddRectFilled({a.x - thickness * 0.5f, std::min(a.y, b.y),
                       thickness, len},
                      color);
        return;
    }
    // Rotated quad: four corners around the line.
    Vec2 n{-delta.y / len, delta.x / len};
    f32 h = thickness * 0.5f;
    Vec2 p0{a.x + n.x * h, a.y + n.y * h};
    Vec2 p1{a.x - n.x * h, a.y - n.y * h};
    Vec2 p2{b.x - n.x * h, b.y - n.y * h};
    Vec2 p3{b.x + n.x * h, b.y + n.y * h};
    Vec2 pts[4] = {p0, p1, p2, p3};
    AddConvexPolyFilled(pts, 4, color);
}

void DrawList::AddCircleFilled(Vec2 center, f32 radius, Color color,
                               int segments) {
    if (segments < 3) segments = 3;
    std::vector<Vec2> pts(static_cast<usize>(segments));
    for (int i = 0; i < segments; ++i) {
        f32 a = (static_cast<f32>(i) / segments) * kPi * 2.0f;
        pts[static_cast<usize>(i)] = {center.x + std::cos(a) * radius,
                                       center.y + std::sin(a) * radius};
    }
    AddConvexPolyFilled(pts.data(), segments, color);
}

void DrawList::AddRectShadow(const Rect& rect, f32 radius, f32 intensity,
                             f32 spread) {
    if (spread <= 0.0f || intensity <= 0.0f) return;
    constexpr int kLayers = 4;
    // Each layer is a slightly larger rounded rect with falling alpha. The
    // first layer hugs the source rect so the shadow feels "attached".
    for (int i = 0; i < kLayers; ++i) {
        f32 t = static_cast<f32>(i + 1) / static_cast<f32>(kLayers);
        f32 pad = spread * t;
        f32 alpha = intensity * (1.0f - t) * (1.0f / kLayers) * 4.0f;
        u8 a = static_cast<u8>(std::clamp(alpha * 255.0f, 0.0f, 255.0f) + 0.5f);
        Rect r{rect.x - pad, rect.y - pad, rect.w + 2.0f * pad,
               rect.h + 2.0f * pad};
        AddRectFilledRounded(r, Color::RGBA(0, 0, 0, a), radius + pad);
    }
}

void DrawList::AddImage(TextureHandle texture, const Rect& dst, const Rect& uv,
                        Color tint) {
    AddQuad(texture, dst, uv, tint);
}

void DrawList::AddImageRotated(TextureHandle texture, Vec2 center, f32 half,
                               f32 rotationRad, Color color,
                               const Rect& uv) {
    EnsureCommand(texture);
    u32 base = static_cast<u32>(m_vertices.size());
    u32 packed = color.Packed();
    // Standard UV corners (TL, TR, BR, BL) of the source rect.
    f32 u0 = uv.x, u1 = uv.Right(), v0 = uv.y, v1 = uv.Bottom();
    struct Corner { Vec2 pos; f32 u, v; };
    Corner corners[4] = {
        {{-half, -half}, u0, v0},  // TL
        {{+half, -half}, u1, v0},  // TR
        {{+half, +half}, u1, v1},  // BR
        {{-half, +half}, u0, v1},  // BL
    };
    f32 c = std::cos(rotationRad);
    f32 s = std::sin(rotationRad);
    for (int i = 0; i < 4; ++i) {
        Vec2 p = corners[i].pos;
        Vec2 rp{p.x * c - p.y * s, p.x * s + p.y * c};
        m_vertices.push_back({center.x + rp.x, center.y + rp.y, corners[i].u,
                              corners[i].v, packed});
    }
    u32 quad[6] = {base, base + 1, base + 2, base + 2, base + 3, base};
    for (u32 i : quad) m_indices.push_back(i);
    m_commands.back().indexCount += 6;
}

void DrawList::AddText(const Font& font, Vec2 pos, std::string_view text,
                       Color color) {
    if (!font.Valid()) return;
    TextureHandle atlas = font.Atlas();
    font.Layout(pos, text, [&](const Rect& dst, const Rect& uv) {
        AddQuad(atlas, dst, uv, color);
    });
}

const UIDrawData& DrawList::Build() {
    m_data.vertices = m_vertices.data();
    m_data.vertexCount = static_cast<u32>(m_vertices.size());
    m_data.indices = m_indices.data();
    m_data.indexCount = static_cast<u32>(m_indices.size());
    m_data.commands = m_commands.data();
    m_data.commandCount = static_cast<u32>(m_commands.size());
    m_data.displayWidth = m_displayWidth;
    m_data.displayHeight = m_displayHeight;
    return m_data;
}

}  // namespace Luma::Slate
