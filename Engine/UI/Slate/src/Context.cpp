#include "Luma/Slate/Context.h"

#include <algorithm>
#include <cmath>

namespace Luma::Slate {
namespace {

// GLFW key codes we care about for text editing (avoids a GLFW dependency here).
constexpr int kKeyEnter = 257;
constexpr int kKeyBackspace = 259;
constexpr int kKeyDelete = 261;
constexpr int kKeyRight = 262;
constexpr int kKeyLeft = 263;
constexpr int kKeyHome = 268;
constexpr int kKeyEnd = 269;

}  // namespace

Theme DarkTheme() {
    Theme t;
    t.windowBg = Color::RGB(24, 26, 31);
    t.panelBg = Color::RGB(32, 35, 42);
    t.panelBorder = Color::RGB(48, 52, 61);
    t.header = Color::RGB(20, 22, 27);
    t.button = Color::RGB(48, 52, 61);
    t.buttonHover = Color::RGB(60, 66, 78);
    t.buttonActive = Color::RGB(40, 44, 52);
    t.buttonText = Color::RGB(225, 228, 235);
    t.text = Color::RGB(228, 231, 238);
    t.textDim = Color::RGB(140, 148, 162);
    t.accent = Color::RGB(50, 140, 220);
    t.accentText = Color::RGB(245, 249, 255);
    t.fieldBg = Color::RGB(18, 20, 24);
    t.fieldBorder = Color::RGB(58, 63, 74);
    t.cardBg = Color::RGB(38, 42, 50);
    t.cardHover = Color::RGB(46, 51, 61);
    t.cardSelected = Color::RGB(30, 78, 120);
    t.caret = Color::RGB(230, 233, 240);
    t.rounding = 6.0f;
    return t;
}

bool Context::Init(Renderer& renderer, const std::string& fontPath,
                   const std::string& titleFontPath, f32 baseSize,
                   f32 titleSize) {
    const std::string& titlePath =
        titleFontPath.empty() ? fontPath : titleFontPath;
    bool ok = m_font.LoadFromFile(renderer, fontPath, baseSize);
    ok = m_titleFont.LoadFromFile(renderer, titlePath, titleSize) && ok;
    return ok;
}

u64 Context::ID(std::string_view s) {
    u64 hash = 1469598103934665603ull;
    for (char c : s) {
        hash ^= static_cast<u8>(c);
        hash *= 1099511628211ull;
    }
    return hash ? hash : 1;
}

void Context::OnMouseMove(f32 x, f32 y) { m_mouse = {x, y}; }

void Context::OnMouseButton(int button, bool down) {
    if (button < 0 || button > 2) return;
    if (down && !m_mouseDown[button]) m_mousePressed[button] = true;
    if (!down && m_mouseDown[button]) m_mouseReleased[button] = true;
    m_mouseDown[button] = down;
}

void Context::OnScroll(f32 y) { m_scroll += y; }

void Context::OnText(u32 codepoint) {
    if (codepoint >= 32 && codepoint < 127) {
        m_textInput.push_back(static_cast<char>(codepoint));
    }
}

void Context::OnKey(int glfwKey, bool down) {
    if (!down) return;
    switch (glfwKey) {
        case kKeyBackspace: m_keyBackspace = true; break;
        case kKeyDelete:    m_keyDelete = true; break;
        case kKeyLeft:      m_keyLeft = true; break;
        case kKeyRight:     m_keyRight = true; break;
        case kKeyHome:      m_keyHome = true; break;
        case kKeyEnd:       m_keyEnd = true; break;
        case kKeyEnter:     m_keyEnter = true; break;
        default: break;
    }
}

void Context::BeginFrame(f32 displayWidth, f32 displayHeight, f32 dt) {
    m_displayW = displayWidth;
    m_displayH = displayHeight;
    m_time += dt;
    m_hot = 0;
    m_mouseDelta = {m_mouse.x - m_prevMouse.x, m_mouse.y - m_prevMouse.y};
    m_prevMouse = m_mouse;
    m_draw.Begin(displayWidth, displayHeight);
}

const UIDrawData& Context::EndFrame() {
    // Clear per-frame input accumulators.
    for (int i = 0; i < 3; ++i) {
        m_mousePressed[i] = false;
        m_mouseReleased[i] = false;
    }
    m_textInput.clear();
    m_keyBackspace = m_keyDelete = m_keyLeft = m_keyRight = false;
    m_keyHome = m_keyEnd = m_keyEnter = false;
    m_scroll = 0.0f;
    return m_draw.Build();
}

void Context::Panel(const Rect& rect, Color color) {
    m_draw.AddRectFilled(rect, color);
}

void Context::PanelBordered(const Rect& rect, Color fill, Color border,
                            f32 thickness) {
    m_draw.AddRectFilled(rect, fill);
    m_draw.AddRectOutline(rect, border, thickness);
}

void Context::PanelRounded(const Rect& rect, Color color, f32 radius) {
    m_draw.AddRectFilledRounded(rect, color, radius);
}

void Context::PanelRoundedBordered(const Rect& rect, Color fill, Color border,
                                   f32 radius, f32 thickness) {
    m_draw.AddRectFilledRounded(rect, border, radius);
    m_draw.AddRectFilledRounded(rect.Inset(thickness, thickness), fill,
                                radius - thickness);
}

void Context::GradientRect(const Rect& rect, Color top, Color bottom) {
    m_draw.AddRectFilledGradient(rect, top, bottom);
}

void Context::Triangle(Vec2 a, Vec2 b, Vec2 c, Color color) {
    m_draw.AddTriangle(a, b, c, color);
}

void Context::LogoMark(Vec2 center, f32 radius) {
    // A four-facet gem: top/right/bottom/left points around the center, with
    // alternating accent shades to read as a lit prism.
    f32 r = radius;
    Vec2 top{center.x, center.y - r};
    Vec2 right{center.x + r * 0.72f, center.y};
    Vec2 bottom{center.x, center.y + r};
    Vec2 left{center.x - r * 0.72f, center.y};
    Color a = Color::RGB(64, 156, 240);
    Color b = Color::RGB(42, 118, 200);
    Color c = Color::RGB(90, 180, 255);
    Color d = Color::RGB(30, 96, 170);
    m_draw.AddTriangle(top, right, center, a);
    m_draw.AddTriangle(right, bottom, center, b);
    m_draw.AddTriangle(bottom, left, center, d);
    m_draw.AddTriangle(left, top, center, c);
}

void Context::Image(TextureHandle texture, const Rect& rect, Color tint) {
    m_draw.AddImage(texture, rect, Rect{0.0f, 0.0f, 1.0f, 1.0f}, tint);
}

void Context::ImageUV(TextureHandle texture, const Rect& rect, const Rect& uv,
                      Color tint) {
    m_draw.AddImage(texture, rect, uv, tint);
}

void Context::Label(Vec2 pos, std::string_view text, Color color) {
    m_draw.AddText(m_font, pos, text, color);
}

void Context::LabelIn(const Rect& rect, std::string_view text, Color color,
                      Align align, bool title) {
    Font& f = title ? m_titleFont : m_font;
    Vec2 size = f.Measure(text);
    f32 x = rect.x + 8.0f;
    if (align == Align::Center) x = rect.x + (rect.w - size.x) * 0.5f;
    else if (align == Align::Right) x = rect.Right() - size.x - 8.0f;
    f32 y = rect.y + (rect.h - f.LineHeight()) * 0.5f;
    m_draw.PushClip(rect);
    m_draw.AddText(f, {x, y}, text, color);
    m_draw.PopClip();
}

bool Context::Button(u64 id, const Rect& rect, std::string_view label) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) m_hot = id;
    if (hovered && m_mousePressed[0]) m_active = id;
    bool clicked = false;
    if (m_active == id && m_mouseReleased[0]) {
        if (hovered) clicked = true;
        m_active = 0;
    }

    Color bg = m_theme.button;
    if (m_active == id) bg = m_theme.buttonActive;
    else if (hovered) bg = m_theme.buttonHover;
    m_draw.AddRectFilledRounded(rect, bg, m_theme.rounding);

    Vec2 size = m_font.Measure(label);
    Vec2 pos{rect.x + (rect.w - size.x) * 0.5f,
             rect.y + (rect.h - m_font.LineHeight()) * 0.5f};
    m_draw.AddText(m_font, pos, label, m_theme.buttonText);
    return clicked;
}

bool Context::Tab(u64 id, const Rect& rect, std::string_view label,
                  bool active) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) m_hot = id;
    bool clicked = false;
    if (hovered && m_mousePressed[0]) m_active = id;
    if (m_active == id && m_mouseReleased[0]) {
        if (hovered) clicked = true;
        m_active = 0;
    }

    Color bg = active ? m_theme.panelBg : (hovered ? m_theme.buttonHover
                                                    : m_theme.header);
    if (active || hovered) {
        m_draw.AddRectFilledRounded(rect, bg, m_theme.rounding);
    }
    if (active) {
        m_draw.AddRectFilled({rect.x + 14.0f, rect.Bottom() - 2.0f,
                              rect.w - 28.0f, 2.0f},
                             m_theme.accent);
    }
    Vec2 size = m_font.Measure(label);
    Vec2 pos{rect.x + (rect.w - size.x) * 0.5f,
             rect.y + (rect.h - m_font.LineHeight()) * 0.5f};
    m_draw.AddText(m_font, pos, label,
                   active ? m_theme.text : m_theme.textDim);
    return clicked;
}

bool Context::TextField(u64 id, const Rect& rect, std::string& text,
                        std::string_view placeholder) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) m_hot = id;
    if (m_mousePressed[0]) {
        if (hovered) {
            m_focus = id;
            m_caret = text.size();
        } else if (m_focus == id) {
            m_focus = 0;
        }
    }

    bool changed = false;
    if (m_focus == id) {
        if (!m_textInput.empty()) {
            if (m_caret > text.size()) m_caret = text.size();
            text.insert(m_caret, m_textInput);
            m_caret += m_textInput.size();
            changed = true;
        }
        if (m_keyBackspace && m_caret > 0) {
            text.erase(m_caret - 1, 1);
            m_caret--;
            changed = true;
        }
        if (m_keyDelete && m_caret < text.size()) {
            text.erase(m_caret, 1);
            changed = true;
        }
        if (m_keyLeft && m_caret > 0) m_caret--;
        if (m_keyRight && m_caret < text.size()) m_caret++;
        if (m_keyHome) m_caret = 0;
        if (m_keyEnd) m_caret = text.size();
    }

    Color border = (m_focus == id) ? m_theme.accent : m_theme.fieldBorder;
    m_draw.AddRectFilledRounded(rect, border, m_theme.rounding);
    m_draw.AddRectFilledRounded(rect.Inset(1.0f, 1.0f), m_theme.fieldBg,
                                m_theme.rounding - 1.0f);

    Vec2 tp{rect.x + 8.0f, rect.y + (rect.h - m_font.LineHeight()) * 0.5f};
    m_draw.PushClip({rect.x + 4.0f, rect.y, rect.w - 8.0f, rect.h});
    if (text.empty() && m_focus != id) {
        m_draw.AddText(m_font, tp, placeholder, m_theme.textDim);
    } else {
        m_draw.AddText(m_font, tp, text, m_theme.text);
        if (m_focus == id && std::fmod(m_time, 1.0f) < 0.5f) {
            usize caret = m_caret <= text.size() ? m_caret : text.size();
            Vec2 w = m_font.Measure(std::string_view(text).substr(0, caret));
            m_draw.AddRectFilled({tp.x + w.x, rect.y + 6.0f, 1.5f, rect.h - 12.0f},
                                 m_theme.caret);
        }
    }
    m_draw.PopClip();
    return changed;
}

bool Context::Card(u64 id, const Rect& rect, std::string_view title,
                   std::string_view desc, bool selected) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) m_hot = id;
    bool clicked = false;
    if (hovered && m_mousePressed[0]) m_active = id;
    if (m_active == id && m_mouseReleased[0]) {
        if (hovered) clicked = true;
        m_active = 0;
    }

    Color bg = selected ? m_theme.cardSelected
                        : (hovered ? m_theme.cardHover : m_theme.cardBg);
    Color border = selected ? m_theme.accent : m_theme.panelBorder;
    f32 t = selected ? 2.0f : 1.0f;
    m_draw.AddRectFilledRounded(rect, border, m_theme.rounding);
    m_draw.AddRectFilledRounded(rect.Inset(t, t), bg, m_theme.rounding - t);

    m_draw.PushClip(rect.Inset(10.0f, 8.0f));
    Color titleColor = selected ? m_theme.accentText : m_theme.text;
    m_draw.AddText(m_font, {rect.x + 12.0f, rect.Bottom() - 48.0f}, title,
                   titleColor);
    m_draw.AddText(m_font, {rect.x + 12.0f, rect.Bottom() - 26.0f}, desc,
                   m_theme.textDim);
    m_draw.PopClip();
    return clicked;
}

bool Context::ImageCard(u64 id, const Rect& rect, TextureHandle image,
                        bool selected) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) m_hot = id;
    bool clicked = false;
    if (hovered && m_mousePressed[0]) m_active = id;
    if (m_active == id && m_mouseReleased[0]) {
        if (hovered) clicked = true;
        m_active = 0;
    }

    Color border = selected ? m_theme.accent
                            : (hovered ? m_theme.textDim : m_theme.panelBorder);
    f32 t = selected ? 3.0f : 1.0f;
    m_draw.AddRectFilledRounded(rect, border, m_theme.rounding);
    if (image) {
        m_draw.AddImage(image, rect.Inset(t, t), Rect{0.0f, 0.0f, 1.0f, 1.0f},
                        Color::RGB(255, 255, 255));
    } else {
        m_draw.AddRectFilledRounded(rect.Inset(t, t), m_theme.cardBg,
                                    m_theme.rounding - t);
    }
    return clicked;
}

bool Context::IconButton(u64 id, const Rect& rect, TextureHandle icon) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) m_hot = id;
    bool clicked = false;
    if (hovered && m_mousePressed[0]) m_active = id;
    if (m_active == id && m_mouseReleased[0]) {
        if (hovered) clicked = true;
        m_active = 0;
    }
    Color bg = m_theme.button;
    if (m_active == id) bg = m_theme.buttonActive;
    else if (hovered) bg = m_theme.buttonHover;
    m_draw.AddRectFilledRounded(rect, bg, m_theme.rounding);
    if (icon) {
        f32 s = std::min(rect.w, rect.h) - 10.0f;
        Rect ir{rect.x + (rect.w - s) * 0.5f, rect.y + (rect.h - s) * 0.5f, s,
                s};
        m_draw.AddImage(icon, ir, Rect{0.0f, 0.0f, 1.0f, 1.0f},
                        Color::RGB(255, 255, 255));
    }
    return clicked;
}

bool Context::Selectable(u64 id, const Rect& rect, std::string_view label,
                         bool selected) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) m_hot = id;
    bool clicked = false;
    if (hovered && m_mousePressed[0]) m_active = id;
    if (m_active == id && m_mouseReleased[0]) {
        if (hovered) clicked = true;
        m_active = 0;
    }
    if (selected) {
        m_draw.AddRectFilledRounded(rect, m_theme.cardSelected, m_theme.rounding);
    } else if (hovered) {
        m_draw.AddRectFilledRounded(rect, m_theme.buttonHover, m_theme.rounding);
    }
    m_draw.AddText(m_font,
                   {rect.x + 10.0f, rect.y + (rect.h - m_font.LineHeight()) * 0.5f},
                   label, selected ? m_theme.accentText : m_theme.text);
    return clicked;
}

bool Context::Checkbox(u64 id, const Rect& box, std::string_view label,
                       bool& value) {
    // Clickable region spans the box plus the label.
    f32 labelW = m_font.Measure(label).x;
    Rect hitRect{box.x, box.y, box.w + 8.0f + labelW, box.h};
    bool hovered = hitRect.Contains(m_mouse);
    if (hovered) m_hot = id;
    bool changed = false;
    if (hovered && m_mousePressed[0]) m_active = id;
    if (m_active == id && m_mouseReleased[0]) {
        if (hovered) {
            value = !value;
            changed = true;
        }
        m_active = 0;
    }

    m_draw.AddRectFilled(box, m_theme.fieldBg);
    m_draw.AddRectOutline(box, hovered ? m_theme.accent : m_theme.fieldBorder,
                          1.0f);
    if (value) {
        m_draw.AddRectFilled(box.Inset(4.0f, 4.0f), m_theme.accent);
    }
    m_draw.AddText(m_font, {box.Right() + 8.0f,
                           box.y + (box.h - m_font.LineHeight()) * 0.5f},
                   label, m_theme.text);
    return changed;
}

bool Context::CollapsingHeader(u64 id, const Rect& rect, std::string_view label,
                               bool& open) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) m_hot = id;
    bool clicked = false;
    if (hovered && m_mousePressed[0]) m_active = id;
    if (m_active == id && m_mouseReleased[0]) {
        if (hovered) {
            open = !open;
            clicked = true;
        }
        m_active = 0;
    }

    m_draw.AddRectFilledRounded(
        rect, hovered ? m_theme.buttonHover : m_theme.button, m_theme.rounding);
    // Disclosure triangle.
    f32 cy = rect.y + rect.h * 0.5f;
    f32 cx = rect.x + 14.0f;
    if (open) {
        m_draw.AddTriangle({cx - 5, cy - 3}, {cx + 5, cy - 3}, {cx, cy + 4},
                           m_theme.textDim);
    } else {
        m_draw.AddTriangle({cx - 3, cy - 5}, {cx + 4, cy}, {cx - 3, cy + 5},
                           m_theme.textDim);
    }
    m_draw.AddText(m_font, {rect.x + 28.0f,
                           rect.y + (rect.h - m_font.LineHeight()) * 0.5f},
                   label, m_theme.text);
    return clicked;
}

bool Context::SplitterV(u64 id, const Rect& region, f32& ratio, Rect& left,
                        Rect& right, f32 thickness) {
    f32 splitX = region.x + region.w * ratio;
    f32 half = thickness * 0.5f;
    f32 hitHalf = std::max(half, 4.0f);  // wider invisible grab zone
    Rect hit{splitX - hitHalf, region.y, hitHalf * 2.0f, region.h};
    bool hovered = hit.Contains(m_mouse);
    if (hovered) m_hot = id;
    if (hovered && m_mousePressed[0]) m_active = id;

    bool dragging = (m_active == id);
    if (dragging) {
        if (!m_mouseDown[0]) {
            m_active = 0;
        } else if (region.w > 1.0f) {
            ratio = std::clamp((m_mouse.x - region.x) / region.w, 0.12f, 0.88f);
            splitX = region.x + region.w * ratio;
        }
    }

    // Panels butt together (shared edge); a 1px line marks the boundary.
    left = {region.x, region.y, splitX - region.x, region.h};
    right = {splitX, region.y, region.Right() - splitX, region.h};
    m_draw.AddRectFilled({splitX - half, region.y, thickness, region.h},
                         (dragging || hovered) ? m_theme.accent
                                               : m_theme.panelBorder);
    return dragging;
}

bool Context::SplitterH(u64 id, const Rect& region, f32& ratio, Rect& top,
                        Rect& bottom, f32 thickness) {
    f32 splitY = region.y + region.h * ratio;
    f32 half = thickness * 0.5f;
    f32 hitHalf = std::max(half, 4.0f);
    Rect hit{region.x, splitY - hitHalf, region.w, hitHalf * 2.0f};
    bool hovered = hit.Contains(m_mouse);
    if (hovered) m_hot = id;
    if (hovered && m_mousePressed[0]) m_active = id;

    bool dragging = (m_active == id);
    if (dragging) {
        if (!m_mouseDown[0]) {
            m_active = 0;
        } else if (region.h > 1.0f) {
            ratio = std::clamp((m_mouse.y - region.y) / region.h, 0.12f, 0.88f);
            splitY = region.y + region.h * ratio;
        }
    }

    top = {region.x, region.y, region.w, splitY - region.y};
    bottom = {region.x, splitY, region.w, region.Bottom() - splitY};
    m_draw.AddRectFilled({region.x, splitY - half, region.w, thickness},
                         (dragging || hovered) ? m_theme.accent
                                               : m_theme.panelBorder);
    return dragging;
}

Rect Context::PanelWithTitle(const Rect& rect, std::string_view title) {
    // Flush docked panel (Unity-style): square, full-bleed, header strip + a
    // 1px divider. Adjacent panels butt together with only the splitter seam.
    const f32 headerH = 28.0f;
    m_draw.AddRectFilled(rect, m_theme.panelBg);
    m_draw.AddRectFilled({rect.x, rect.y, rect.w, headerH}, m_theme.header);
    m_draw.AddText(
        m_font, {rect.x + 12.0f, rect.y + (headerH - m_font.LineHeight()) * 0.5f},
        title, m_theme.text);
    m_draw.AddRectFilled({rect.x, rect.y + headerH, rect.w, 1.0f},
                         m_theme.panelBorder);
    return Rect{rect.x, rect.y + headerH + 1.0f, rect.w, rect.h - headerH - 1.0f};
}

}  // namespace Luma::Slate
