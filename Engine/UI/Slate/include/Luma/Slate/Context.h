#pragma once

#include <string>
#include <string_view>

#include "Luma/RHI/Renderer.h"
#include "Luma/Slate/DrawList.h"
#include "Luma/Slate/Font.h"
#include "Luma/Slate/Types.h"

// Luma Slate immediate-mode UI context. The app feeds input each frame, then
// calls widget functions between BeginFrame/EndFrame; EndFrame returns the
// UIDrawData to hand to Renderer::DrawUI.

namespace Luma::Slate {

struct Theme {
    Color windowBg;
    Color panelBg;
    Color panelBorder;
    Color header;
    Color button;
    Color buttonHover;
    Color buttonActive;
    Color buttonText;
    Color text;
    Color textDim;
    Color accent;
    Color accentText;
    Color fieldBg;
    Color fieldBorder;
    Color cardBg;
    Color cardHover;
    Color cardSelected;
    Color caret;
    f32 rounding = 6.0f;
};

Theme DarkTheme();

// Horizontal text alignment for LabelIn.
enum class Align { Left, Center, Right };

class Context {
public:
    // Loads the base + title fonts through the renderer. If titleFontPath is
    // empty the base font is reused for titles. Returns false on error.
    bool Init(Renderer& renderer, const std::string& fontPath,
              const std::string& titleFontPath = {}, f32 baseSize = 18.0f,
              f32 titleSize = 30.0f);

    // --- Input feed (call from the app's event callback) --------------------
    void OnMouseMove(f32 x, f32 y);
    void OnMouseButton(int button, bool down);  // 0=left,1=right,2=middle
    void OnScroll(f32 y);
    void OnText(u32 codepoint);
    void OnKey(int glfwKey, bool down);

    // --- Frame --------------------------------------------------------------
    void BeginFrame(f32 displayWidth, f32 displayHeight, f32 dt);
    const UIDrawData& EndFrame();

    // --- Widgets ------------------------------------------------------------
    void Panel(const Rect& rect, Color color);
    void PanelBordered(const Rect& rect, Color fill, Color border,
                       f32 thickness = 1.0f);
    void PanelRounded(const Rect& rect, Color color, f32 radius);
    void PanelRoundedBordered(const Rect& rect, Color fill, Color border,
                              f32 radius, f32 thickness = 1.0f);
    void GradientRect(const Rect& rect, Color top, Color bottom);
    void Triangle(Vec2 a, Vec2 b, Vec2 c, Color color);
    void Image(TextureHandle texture, const Rect& rect,
               Color tint = Color::RGB(255, 255, 255));
    void ImageUV(TextureHandle texture, const Rect& rect, const Rect& uv,
                 Color tint = Color::RGB(255, 255, 255));
    // Draws the Luma gem/prism logo mark centered at `center`.
    void LogoMark(Vec2 center, f32 radius);
    void Label(Vec2 pos, std::string_view text, Color color);
    void LabelIn(const Rect& rect, std::string_view text, Color color,
                 Align align = Align::Left, bool title = false);
    bool Button(u64 id, const Rect& rect, std::string_view label);
    bool Tab(u64 id, const Rect& rect, std::string_view label, bool active);
    bool TextField(u64 id, const Rect& rect, std::string& text,
                   std::string_view placeholder = {});
    bool Card(u64 id, const Rect& rect, std::string_view title,
              std::string_view desc, bool selected);
    // A selectable card whose content is a single image (e.g. a template
    // thumbnail); shows a selection/hover border.
    bool ImageCard(u64 id, const Rect& rect, TextureHandle image, bool selected);
    // A button showing a centered icon.
    bool IconButton(u64 id, const Rect& rect, TextureHandle icon);
    bool Checkbox(u64 id, const Rect& box, std::string_view label, bool& value);
    // A collapsible section header; toggles `open` when clicked.
    bool CollapsingHeader(u64 id, const Rect& rect, std::string_view label,
                          bool& open);

    // --- Docking primitives -------------------------------------------------
    // Splits `region` by `ratio` (0..1) with a draggable divider. Outputs the
    // two sub-regions; updates `ratio` while dragging. Returns true if dragging.
    bool SplitterV(u64 id, const Rect& region, f32& ratio, Rect& left,
                   Rect& right, f32 thickness = 6.0f);
    bool SplitterH(u64 id, const Rect& region, f32& ratio, Rect& top,
                   Rect& bottom, f32 thickness = 6.0f);
    // Draws a docked panel (background, title bar, border); returns the content
    // rect below the title bar.
    Rect PanelWithTitle(const Rect& rect, std::string_view title);

    // A full-width selectable list row (used by the outliner).
    bool Selectable(u64 id, const Rect& rect, std::string_view label,
                    bool selected);

    Theme& theme() { return m_theme; }
    Font& font() { return m_font; }
    Font& titleFont() { return m_titleFont; }
    Vec2 mouse() const { return m_mouse; }
    Vec2 mouseDelta() const { return m_mouseDelta; }
    f32 scrollDelta() const { return m_scroll; }
    bool isMouseDown(int button) const {
        return button >= 0 && button < 3 ? m_mouseDown[button] : false;
    }

    // FNV-1a hash for stable widget ids from string literals.
    static u64 ID(std::string_view s);

private:
    DrawList m_draw;
    Font m_font;
    Font m_titleFont;
    Theme m_theme = DarkTheme();

    Vec2 m_mouse;
    Vec2 m_prevMouse;
    Vec2 m_mouseDelta;
    bool m_mouseDown[3] = {};
    bool m_mousePressed[3] = {};
    bool m_mouseReleased[3] = {};
    f32 m_scroll = 0.0f;
    std::string m_textInput;
    bool m_keyBackspace = false, m_keyDelete = false;
    bool m_keyLeft = false, m_keyRight = false;
    bool m_keyHome = false, m_keyEnd = false, m_keyEnter = false;

    u64 m_hot = 0;
    u64 m_active = 0;
    u64 m_focus = 0;
    usize m_caret = 0;
    f32 m_time = 0.0f;
    f32 m_displayW = 0.0f;
    f32 m_displayH = 0.0f;
};

}  // namespace Luma::Slate
