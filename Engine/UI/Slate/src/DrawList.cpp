#include "Luma/Slate/DrawList.h"

namespace Luma::Slate {

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

void DrawList::PushClip(Rect clip) { m_clipStack.push_back(clip); }
void DrawList::PopClip() {
    if (!m_clipStack.empty()) m_clipStack.pop_back();
}

void DrawList::AddQuad(TextureHandle texture, const Rect& dst, const Rect& uv,
                       Color color) {
    Rect clip = CurrentClip();

    // Start a new command if texture or clip differs from the last one.
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

void DrawList::AddRectOutline(const Rect& rect, Color color, f32 t) {
    AddRectFilled(Rect{rect.x, rect.y, rect.w, t}, color);                  // top
    AddRectFilled(Rect{rect.x, rect.Bottom() - t, rect.w, t}, color);       // bottom
    AddRectFilled(Rect{rect.x, rect.y, t, rect.h}, color);                  // left
    AddRectFilled(Rect{rect.Right() - t, rect.y, t, rect.h}, color);        // right
}

void DrawList::AddImage(TextureHandle texture, const Rect& dst, const Rect& uv,
                        Color tint) {
    AddQuad(texture, dst, uv, tint);
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
