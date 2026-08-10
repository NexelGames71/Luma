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

    void PushClip(Rect clip);
    void PopClip();

    void AddRectFilled(const Rect& rect, Color color);
    void AddRectFilledGradient(const Rect& rect, Color top, Color bottom);
    void AddRectOutline(const Rect& rect, Color color, f32 thickness = 1.0f);
    void AddTriangle(Vec2 a, Vec2 b, Vec2 c, Color color);
    void AddImage(TextureHandle texture, const Rect& dst, const Rect& uv,
                  Color tint);
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
