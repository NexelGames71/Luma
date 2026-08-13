#include "Luma/Slate/Context.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Luma/Slate/Theme.h"

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

bool Context::Init(Renderer& renderer, const std::string& fontPath,
                   const std::string& titleFontPath, f32 baseSize,
                   f32 titleSize, const std::string& uiFontPath, f32 uiSize,
                   const std::string& mediumFontPath, f32 mediumSize,
                   const std::string& monoFontPath, f32 monoSize,
                   f32 dpiScale) {
    m_dpiScale = dpiScale > 0.0f ? dpiScale : 1.0f;
    const std::string& titlePath =
        titleFontPath.empty() ? fontPath : titleFontPath;
    const std::string& uiPath = uiFontPath.empty() ? titlePath : uiFontPath;
    const std::string& mediumPath =
        mediumFontPath.empty() ? uiPath : mediumFontPath;
    const std::string& monoPath = monoFontPath.empty() ? fontPath : monoFontPath;
    const f32 s = m_dpiScale;
    bool ok = m_font.LoadFromFile(renderer, fontPath, baseSize * s);
    ok = m_titleFont.LoadFromFile(renderer, titlePath, titleSize * s) && ok;
    ok = m_uiFont.LoadFromFile(renderer, uiPath, uiSize * s) && ok;
    ok = m_mediumFont.LoadFromFile(renderer, mediumPath, mediumSize * s) && ok;
    ok = m_monoFont.LoadFromFile(renderer, monoPath, monoSize * s) && ok;
    return ok;
}

bool Context::Init(Renderer& renderer, const Typography& type, f32 dpiScale) {
    m_dpiScale = dpiScale > 0.0f ? dpiScale : 1.0f;
    // Persist the typography on the theme so widgets can read sizes/weights
    // from `theme().type` (design-token source of truth).
    m_theme.type = type;

    // Fallback chain: regular -> medium -> semiBold -> bold. Each weight uses
    // the previous non-empty weight's path if its own is missing.
    const std::string& reg = type.uiRegular;
    const std::string& med = type.uiMedium.empty() ? reg : type.uiMedium;
    const std::string& sb = type.uiSemiBold.empty() ? med : type.uiSemiBold;
    const std::string& bd = type.uiBold.empty() ? sb : type.uiBold;
    const std::string& mono = type.mono.empty() ? reg : type.mono;

    // Map design roles to Slate's internal font slots. Sizes come from the
    // typography scale so the whole product stays consistent. Bake at
    // pixelSize * dpiScale so glyphs stay crisp on hi-DPI displays.
    const f32 s = m_dpiScale;
    bool ok = m_font.LoadFromFile(renderer, reg, type.bodySize * s);
    ok = m_mediumFont.LoadFromFile(renderer, med, type.captionSize * s) && ok;
    ok = m_uiFont.LoadFromFile(renderer, sb, type.headingSize * s) && ok;
    ok = m_titleFont.LoadFromFile(renderer, bd, type.titleSize * s) && ok;
    ok = m_monoFont.LoadFromFile(renderer, mono, type.captionSize * s) && ok;
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
    m_requestedCursor = CursorShape::Arrow;
    m_mouseDelta = {m_mouse.x - m_prevMouse.x, m_mouse.y - m_prevMouse.y};
    m_prevMouse = m_mouse;
    m_draw.Begin(displayWidth, displayHeight);
    ++m_frame;
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
    // Evict stale animation entries so the map doesn't grow unbounded.
    for (auto it = m_anim.begin(); it != m_anim.end();) {
        if (m_frame - it->second.lastTouch > kAnimGcAge) {
            it = m_anim.erase(it);
        } else {
            ++it;
        }
    }
    return m_draw.Build();
}

f32 Context::Animate(u64 id, bool active, f32 speed) {
    AnimState& s = m_anim[id];
    s.lastTouch = m_frame;
    f32 target = active ? 1.0f : 0.0f;
    // Exponential lerp: per-second rate, frame-rate independent.
    f32 k = 1.0f - std::exp(-speed * 0.0166f);  // assumes ~60 fps; close enough
    s.value += (target - s.value) * k;
    if (std::fabs(s.value - target) < 1e-4f) s.value = target;
    return s.value;
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

void Context::PushClip(const Rect& rect) { m_draw.PushClip(rect); }
void Context::PopClip() { m_draw.PopClip(); }

void Context::ImageUV(TextureHandle texture, const Rect& rect, const Rect& uv,
                      Color tint) {
    m_draw.AddImage(texture, rect, uv, tint);
}

void Context::Label(Vec2 pos, std::string_view text, Color color) {
    m_draw.AddText(m_font, pos, text, color);
}

void Context::LabelIn(const Rect& rect, std::string_view text, Color color,
                      Align align, bool title) {
    // Title = splash/big Luma wordmark, otherwise the regular body font.
    Font& f = title ? m_titleFont : m_font;
    Vec2 size = f.Measure(text);
    // Token-driven horizontal padding; left-align to the input token (12px).
    const f32 padX = m_theme.space.lg;
    f32 x = rect.x + padX;
    if (align == Align::Center) x = rect.x + (rect.w - size.x) * 0.5f;
    else if (align == Align::Right) x = rect.Right() - size.x - padX;
    f32 y = rect.y + (rect.h - f.LineHeight()) * 0.5f;
    m_draw.PushClip(rect);
    m_draw.AddText(f, {x, y}, text, color);
    m_draw.PopClip();
}

void Context::LabelInMono(const Rect& rect, std::string_view text, Color color,
                          Align align) {
    // Console / log surface; reads from m_monoFont (loaded by Init's mono path).
    Vec2 size = m_monoFont.Measure(text);
    const f32 padX = m_theme.space.lg;
    f32 x = rect.x + padX;
    if (align == Align::Center) x = rect.x + (rect.w - size.x) * 0.5f;
    else if (align == Align::Right) x = rect.Right() - size.x - padX;
    f32 y = rect.y + (rect.h - m_monoFont.LineHeight()) * 0.5f;
    m_draw.PushClip(rect);
    m_draw.AddText(m_monoFont, {x, y}, text, color);
    m_draw.PopClip();
}

void Context::Heading(const Rect& rect, std::string_view text, Color color,
                      Align align) {
    // Headings use the UI (SemiBold) font for visible weight contrast.
    Vec2 size = m_uiFont.Measure(text);
    const f32 padX = m_theme.space.lg;
    f32 x = rect.x + padX;
    if (align == Align::Center) x = rect.x + (rect.w - size.x) * 0.5f;
    else if (align == Align::Right) x = rect.Right() - size.x - padX;
    f32 y = rect.y + (rect.h - m_uiFont.LineHeight()) * 0.5f;
    m_draw.PushClip(rect);
    m_draw.AddText(m_uiFont, {x, y}, text, color);
    m_draw.PopClip();
}

bool Context::Button(u64 id, const Rect& rect, std::string_view label) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) {
        m_hot = id;
        m_requestedCursor = CursorShape::Hand;
    }
    if (hovered && m_mousePressed[0]) m_active = id;
    bool clicked = false;
    if (m_active == id && m_mouseReleased[0]) {
        if (hovered) clicked = true;
        m_active = 0;
    }

    // Subtle hover fade-in via Animate: lerp the button fill toward hover
    // colour while hovered, back to rest when not. Speed from the motion
    // token so hover fades are consistent across the product.
    f32 hoverT = Animate(id ^ 0xBEEFCAFEull, hovered, m_theme.motion.hover);
    Color rest = m_theme.button;
    Color bg = Mix(rest, m_theme.buttonHover, hoverT);
    if (m_active == id) bg = Darken(bg, 0.06f);
    m_draw.AddRectFilledRounded(rect, bg, m_theme.rounding);

    // Focus ring: an accent outline that appears whenever the button is
    // held down (mirrors the focus contract from the design spec).
    if (m_active == id) {
        m_draw.AddRectOutline(rect, m_theme.focusRing,
                              m_theme.border.thick);
    }

    Vec2 size = m_uiFont.Measure(label);
    Vec2 pos{rect.x + (rect.w - size.x) * 0.5f,
             rect.y + (rect.h - m_uiFont.LineHeight()) * 0.5f};
    m_draw.AddText(m_uiFont, pos, label, m_theme.buttonText);
    return clicked;
}

bool Context::MenuButton(u64 id, const Rect& rect, std::string_view label) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) {
        m_hot = id;
        m_requestedCursor = CursorShape::Hand;
    }
    if (hovered && m_mousePressed[0]) m_active = id;
    bool clicked = false;
    if (m_active == id && m_mouseReleased[0]) {
        if (hovered) clicked = true;
        m_active = 0;
    }
    // Menu-bar item: subtle pill on hover, slightly stronger on press. Use the
    // surface ramp so the bar reads as flush chrome, not a button.
    if (m_active == id) {
        m_draw.AddRectFilledRounded(rect, m_theme.buttonActive,
                                    m_theme.radius.sm);
    } else if (hovered) {
        m_draw.AddRectFilledRounded(rect, m_theme.buttonHover,
                                    m_theme.radius.sm);
    }
    Vec2 size = m_uiFont.Measure(label);
    Vec2 pos{rect.x + (rect.w - size.x) * 0.5f,
             rect.y + (rect.h - m_uiFont.LineHeight()) * 0.5f};
    m_draw.AddText(m_uiFont, pos, label, m_theme.text);
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

    // Tabs sit on the panel surface: active gets a raised pill, hover gets a
    // subtle hint. The accent underline below marks the active tab.
    if (active) {
        m_draw.AddRectFilledRounded(rect, m_theme.surface3, m_theme.radius.md);
    } else if (hovered) {
        m_draw.AddRectFilledRounded(rect, m_theme.surface2, m_theme.radius.md);
    }
    if (active) {
        // 2px accent underline (token border.thick), centered under the tab.
        m_draw.AddRectFilled(
            {rect.x + rect.w * 0.25f, rect.Bottom() - 2.0f, rect.w * 0.5f, 2.0f},
            m_theme.accent);
    }
    // Tabs use the UI weight for visible emphasis vs. body text.
    Vec2 size = m_uiFont.Measure(label);
    Vec2 pos{rect.x + (rect.w - size.x) * 0.5f,
             rect.y + (rect.h - m_uiFont.LineHeight()) * 0.5f};
    m_draw.AddText(m_uiFont, pos, label,
                   active ? m_theme.text : m_theme.textDim);
    return clicked;
}

bool Context::TextField(u64 id, const Rect& rect, std::string& text,
                        std::string_view placeholder) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) {
        m_hot = id;
        m_requestedCursor = CursorShape::IBeam;
    }
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

    // Hairline border, field bg inside, token radius. Focus gets a 2px accent
    // outline (never a fill) per the focus-ring convention.
    const f32 ringT = m_theme.border.hairline;
    const f32 focusT = m_theme.border.thick;
    bool focused = (m_focus == id);
    m_draw.AddRectFilledRounded(rect, m_theme.fieldBorder, m_theme.radius.md);
    m_draw.AddRectFilledRounded(rect.Inset(ringT, ringT), m_theme.fieldBg,
                                std::max(0.0f, m_theme.radius.md - ringT));
    if (focused) {
        m_draw.AddRectOutline(rect.Inset(focusT * 0.5f, focusT * 0.5f),
                              m_theme.focusRing, focusT);
    }

    // Input padding uses the spacing token (lg=12).
    const f32 padX = m_theme.space.lg;
    Vec2 tp{rect.x + padX, rect.y + (rect.h - m_font.LineHeight()) * 0.5f};
    m_draw.PushClip(
        {rect.x + padX * 0.5f, rect.y, rect.w - padX, rect.h});
    if (text.empty() && !focused) {
        m_draw.AddText(m_font, tp, placeholder, m_theme.textDim);
    } else {
        m_draw.AddText(m_font, tp, text, m_theme.text);
        if (focused && std::fmod(m_time, 1.0f) < 0.5f) {
            usize caret = m_caret <= text.size() ? m_caret : text.size();
            Vec2 w = m_font.Measure(std::string_view(text).substr(0, caret));
            m_draw.AddRectFilled({tp.x + w.x, rect.y + 6.0f, 1.5f,
                                  rect.h - 12.0f},
                                 m_theme.accent);
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

    // Three-state fill: selected (accentMuted) > hover (surface3) > rest
    // (surface2). Border is hairline by default and a 2px accent when selected.
    // A subtle E1 shadow grounds the card against the panel.
    Color bg = selected
                   ? m_theme.cardSelected
                   : (hovered ? m_theme.cardHover : m_theme.cardBg);
    const f32 borderT = selected ? m_theme.border.thick
                                 : m_theme.border.hairline;
    Color border = selected ? m_theme.accent : m_theme.outline;
    m_draw.AddRectShadow(rect, m_theme.radius.md, 0.20f, 5.0f);
    m_draw.AddRectFilledRounded(rect, border, m_theme.radius.md);
    m_draw.AddRectFilledRounded(rect.Inset(borderT, borderT), bg,
                                std::max(0.0f, m_theme.radius.md - borderT));

    m_draw.PushClip(rect.Inset(m_theme.space.md, m_theme.space.sm));
    Color titleColor = selected ? m_theme.accentText : m_theme.text;
    m_draw.AddText(
        m_uiFont,
        {rect.x + m_theme.space.lg, rect.Bottom() - 46.0f},
        title, titleColor);
    m_draw.AddText(m_font,
                   {rect.x + m_theme.space.lg, rect.Bottom() - 24.0f},
                   desc, m_theme.textDim);
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

    // Selected = 2px accent border; hovered = outline color border; rest =
    // hairline outline against the panel.
    const f32 borderT = selected ? m_theme.border.thick
                                 : m_theme.border.hairline;
    Color border = selected ? m_theme.accent : m_theme.outline;
    m_draw.AddRectFilledRounded(rect, border, m_theme.radius.md);
    if (image) {
        m_draw.AddImage(image, rect.Inset(borderT, borderT),
                        Rect{0.0f, 0.0f, 1.0f, 1.0f}, Color::RGB(255, 255, 255));
    } else {
        m_draw.AddRectFilledRounded(
            rect.Inset(borderT, borderT), m_theme.cardBg,
            std::max(0.0f, m_theme.radius.md - borderT));
    }
    return clicked;
}

bool Context::IconButton(u64 id, const Rect& rect, TextureHandle icon) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) {
        m_hot = id;
        m_requestedCursor = CursorShape::Hand;
    }
    if (hovered && m_mousePressed[0]) m_active = id;
    bool clicked = false;
    if (m_active == id && m_mouseReleased[0]) {
        if (hovered) clicked = true;
        m_active = 0;
    }
    // Same ramp as text buttons for visual consistency across the toolbar.
    Color bg = m_theme.button;
    if (m_active == id) bg = Darken(m_theme.button, 0.06f);
    else if (hovered) bg = m_theme.buttonHover;
    m_draw.AddRectFilledRounded(rect, bg, m_theme.rounding);
    if (icon) {
        // Icon padding from the sizing token (icon 14/16/20, here 14).
        const f32 iconSide = m_theme.size.iconMd;
        f32 s = std::min(rect.w, rect.h) - iconSide;
        s = std::max(s, 1.0f);
        Rect ir{rect.x + (rect.w - s) * 0.5f, rect.y + (rect.h - s) * 0.5f, s,
                s};
        m_draw.AddImage(icon, ir, Rect{0.0f, 0.0f, 1.0f, 1.0f},
                        m_theme.text);
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
    // Outliner rows: selected gets accentMuted (low-chroma), hover gets a
    // subtle surface2 lift so the row reads as interactive.
    if (selected) {
        m_draw.AddRectFilledRounded(rect, m_theme.selectionBg, m_theme.radius.sm);
    } else if (hovered) {
        m_draw.AddRectFilledRounded(rect, m_theme.surface2, m_theme.radius.sm);
    }
    m_draw.AddText(
        m_font,
        {rect.x + m_theme.space.md + m_theme.space.xs,
         rect.y + (rect.h - m_font.LineHeight()) * 0.5f},
        label, selected ? m_theme.selectionText : m_theme.text);
    return clicked;
}

bool Context::Checkbox(u64 id, const Rect& box, std::string_view label,
                       bool& value) {
    // Clickable region spans the box plus the label, with token padding.
    f32 labelW = m_font.Measure(label).x;
    f32 padX = m_theme.space.md;
    Rect hitRect{box.x, box.y, box.w + padX + labelW, box.h};
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

    // Rounded square, hairline border, accent fill when checked. Hover swaps
    // the border to accent as a focus-ring hint.
    m_draw.AddRectFilledRounded(box, m_theme.fieldBg, m_theme.radius.sm);
    m_draw.AddRectOutline(
        Rect{box.x + 0.5f, box.y + 0.5f, box.w - 1.0f, box.h - 1.0f},
        hovered ? m_theme.focusRing : m_theme.fieldBorder,
        m_theme.border.hairline);
    if (value) {
        m_draw.AddRectFilledRounded(
            box.Inset(3.0f, 3.0f), m_theme.accent,
            std::max(0.0f, m_theme.radius.sm - 1.0f));
    }
    m_draw.AddText(m_font, {box.Right() + padX,
                           box.y + (box.h - m_font.LineHeight()) * 0.5f},
                   label, m_theme.text);
    return changed;
}

bool Context::DragFloat(u64 id, const Rect& rect, f32& value, f32 speed) {
    bool hovered = rect.Contains(m_mouse);
    if (hovered) {
        m_hot = id;
        m_requestedCursor = CursorShape::ResizeEW;
    }
    if (hovered && m_mousePressed[0]) m_active = id;

    bool changed = false;
    if (m_active == id) {
        m_requestedCursor = CursorShape::ResizeEW;
        if (!m_mouseDown[0]) {
            m_active = 0;
        } else if (m_mouseDelta.x != 0.0f) {
            value += m_mouseDelta.x * speed;
            changed = true;
        }
    }

    // Hairline border resting; accent hairline when hovered/active (focus
    // ring convention). Field background inside, then value text centered.
    bool active = (m_active == id);
    Color border = (hovered || active) ? m_theme.focusRing
                                       : m_theme.fieldBorder;
    m_draw.AddRectFilledRounded(rect, border, m_theme.radius.md);
    m_draw.AddRectFilledRounded(
        rect.Inset(m_theme.border.hairline, m_theme.border.hairline),
        m_theme.fieldBg,
        std::max(0.0f, m_theme.radius.md - m_theme.border.hairline));

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", value);
    Vec2 ts = m_font.Measure(buf);
    m_draw.PushClip(rect);
    m_draw.AddText(m_font,
                   {rect.x + (rect.w - ts.x) * 0.5f,
                    rect.y + (rect.h - m_font.LineHeight()) * 0.5f},
                   buf, m_theme.text);
    m_draw.PopClip();
    return changed;
}

bool Context::Vector3Field(u64 id, const Rect& rect, f32* xyz) {
    // Channel colors (refined to read as identification, not a primary
    // signal). Text on the chip is white; the value cell uses field bg.
    const char* labels[3] = {"X", "Y", "Z"};
    const Color chipColors[3] = {Color::RGB(196, 78, 82),
                                 Color::RGB(96, 176, 96),
                                 Color::RGB(78, 130, 208)};
    const f32 gap = m_theme.space.sm + 2.0f;
    const f32 cellW = (rect.w - gap * 2.0f) / 3.0f;
    const f32 labelW = 18.0f;
    bool changed = false;
    for (int i = 0; i < 3; ++i) {
        Rect cell{rect.x + static_cast<f32>(i) * (cellW + gap), rect.y, cellW,
                  rect.h};
        m_draw.AddRectFilledRounded({cell.x, cell.y, labelW, cell.h},
                                    chipColors[i], m_theme.radius.sm);
        Vec2 ls = m_font.Measure(labels[i]);
        m_draw.AddText(m_font,
                       {cell.x + (labelW - ls.x) * 0.5f,
                        cell.y + (cell.h - m_font.LineHeight()) * 0.5f},
                       labels[i], m_theme.accentText);
        Rect field{cell.x + labelW + 2.0f, cell.y, cell.w - labelW - 2.0f,
                   cell.h};
        if (DragFloat(id * 4 + static_cast<u64>(i) + 1, field, xyz[i])) {
            changed = true;
        }
    }
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

    // Subtle surface lift on hover; no fill when idle. Hairline border only
    // on hover so the rest state reads as a flat section header.
    if (hovered) {
        m_draw.AddRectFilledRounded(rect, m_theme.surface3,
                                    m_theme.radius.md);
    }
    // Chevron disclosure (token-aligned geometry).
    f32 cy = rect.y + rect.h * 0.5f;
    f32 cx = rect.x + m_theme.space.lg + 2.0f;
    Color chevron = open ? m_theme.text : m_theme.textDim;
    if (open) {
        m_draw.AddTriangle({cx - 4, cy - 2}, {cx + 4, cy - 2}, {cx, cy + 4},
                           chevron);
    } else {
        m_draw.AddTriangle({cx - 2, cy - 4}, {cx + 4, cy}, {cx - 2, cy + 4},
                           chevron);
    }
    m_draw.AddText(
        m_uiFont,
        {rect.x + m_theme.space.lg + 14.0f,
         rect.y + (rect.h - m_uiFont.LineHeight()) * 0.5f},
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
    if (hovered || m_active == id) m_requestedCursor = CursorShape::ResizeEW;
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

    // Panels butt together (shared edge); rest = hairline separator, drag/hover
    // = accent (visible affordance that the divider is grabbable).
    left = {region.x, region.y, splitX - region.x, region.h};
    right = {splitX, region.y, region.Right() - splitX, region.h};
    m_draw.AddRectFilled({splitX - half, region.y, thickness, region.h},
                         (dragging || hovered) ? m_theme.accent
                                               : m_theme.separator);
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
    if (hovered || m_active == id) m_requestedCursor = CursorShape::ResizeNS;
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
                                               : m_theme.separator);
    return dragging;
}

Rect Context::PanelWithTitle(const Rect& rect, std::string_view title) {
    // Flush docked panel with an active tab (Unity/Unreal style): a surface2
    // header strip with a tab chip that matches the body, an accent underline
    // on the title, and a hairline separator between header and body. A
    // subtle E1 shadow lifts the panel off the window surface.
    const f32 headerH = 28.0f;
    m_draw.AddRectShadow(rect, m_theme.radius.md, 0.18f, 4.0f);
    m_draw.AddRectFilled(rect, m_theme.panelBg);
    m_draw.AddRectFilled({rect.x, rect.y, rect.w, headerH}, m_theme.header);

    // Tab chip merges into the body and carries the accent underline.
    f32 tabW = m_uiFont.Measure(title).x + m_theme.space.xl;
    Rect tab{rect.x, rect.y, tabW, headerH};
    m_draw.AddRectFilled(tab, m_theme.panelBg);
    m_draw.AddRectFilled({tab.x + m_theme.space.md, tab.Bottom() - 2.0f,
                          tab.w - m_theme.space.lg, 2.0f},
                         m_theme.accent);
    m_draw.AddText(
        m_uiFont,
        {rect.x + m_theme.space.lg,
         rect.y + (headerH - m_uiFont.LineHeight()) * 0.5f},
        title, m_theme.text);

    m_draw.AddRectFilled({rect.x, rect.y + headerH, rect.w, 1.0f},
                         m_theme.separator);
    return Rect{rect.x, rect.y + headerH + 1.0f, rect.w,
                rect.h - headerH - 1.0f};
}

}  // namespace Luma::Slate
