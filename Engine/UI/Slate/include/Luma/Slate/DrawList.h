#pragma once

#include <string_view>
#include <vector>

#include "Luma/RHI/Renderer.h"
#include "Luma/Slate/Font.h"
#include "Luma/Slate/Types.h"

namespace Luma::Slate {

// Accumulates 2D geometry into an RHI UIDrawData. Draws are batched by
// (texture, clip rect); a new command starts when either changes.
class DrawList {
public:
    void Begin(f32 displayWidth, f32 displayHeight);

    // Live counts (pre-Build) for tests + advanced widgets.
    usize vertexCount() const { return m_vertices.size(); }
    usize indexCount() const { return m_indices.size(); }

    void PushClip(Rect clip);
    void PopClip();

    void AddRectFilled(const Rect& rect, Color color);
    void AddRectFilledRounded(const Rect& rect, Color color, f32 radius);
    void AddRectFilledGradient(const Rect& rect, Color top, Color bottom);
    void AddRectOutline(const Rect& rect, Color color, f32 thickness = 1.0f);
    // Outline of a rounded rect (stroke centered on the perimeter). Used for
    // focus rings so they match the rounded field shape instead of cutting
    // sharp corners across it.
    void AddRectOutlineRounded(const Rect& rect, Color color, f32 thickness,
                               f32 radius);
    void AddConvexPolyFilled(const Vec2* points, int count, Color color);
    void AddTriangle(Vec2 a, Vec2 b, Vec2 c, Color color);
    void AddLine(Vec2 a, Vec2 b, Color color, f32 thickness = 1.0f);
    // Approximated with a regular polygon (default 24 segments).
    void AddCircleFilled(Vec2 center, f32 radius, Color color,
                         int segments = 24);
    // Soft shadow: N stacked, progressively larger, translucent rounded rects.
    // `intensity` 0..1 modulates alpha; `radius` matches the receiver's corner
    // radius; `spread` is the outer reach in px (e.g. 6 = subtle lift).
    void AddRectShadow(const Rect& rect, f32 radius, f32 intensity = 0.5f,
                       f32 spread = 6.0f);
    void AddImage(TextureHandle texture, const Rect& dst, const Rect& uv,
                  Color tint);
    // Draws `texture` as a square of side 2*half centered at `center`,
    // rotated counter-clockwise by `rotationRad` (radians) around its
    // center, tinted with `color`. UVs cover the full texture (0,0)..(1,1)
    // mapped to the four rotated corners so the image reads as rotated.
    void AddImageRotated(TextureHandle texture, Vec2 center, f32 half,
                        f32 rotationRad, Color color,
                        const Rect& uv = Rect{0.0f, 0.0f, 1.0f, 1.0f});
    void AddText(const Font& font, Vec2 pos, std::string_view text, Color color);

    // Finalizes and returns draw data valid until the next Begin().
    const UIDrawData& Build();

private:
    void EnsureCommand(TextureHandle texture);
    void AddQuad(TextureHandle texture, const Rect& dst, const Rect& uv,
                 Color color);
    Rect CurrentClip() const;

    std::vector<UIVertex> m_vertices;
    std::vector<u32> m_indices;
    std::vector<UIDrawCommand> m_commands;
    std::vector<Rect> m_clipStack;

    f32 m_displayWidth = 0.0f;
    f32 m_displayHeight = 0.0f;
    UIDrawData m_data;
};

}  // namespace Luma::Slate
