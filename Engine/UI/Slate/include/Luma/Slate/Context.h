#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Luma/Platform/Cursor.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/Slate/DrawList.h"
#include "Luma/Slate/Font.h"
#include "Luma/Slate/IconKind.h"
#include "Luma/Slate/Theme.h"
#include "Luma/Slate/Types.h"

// Luma Slate immediate-mode UI context. The app feeds input each frame, then
// calls widget functions between BeginFrame/EndFrame; EndFrame returns the
// UIDrawData to hand to Renderer::DrawUI.
//
// Visual constants (colors, sizes, radii, motion) live in Theme.h. The
// Context owns one Theme that widgets read through m_theme.

namespace Luma::Slate {

// Horizontal text alignment for LabelIn.
enum class Align { Left, Center, Right };

class Context {
public:
    // Loads the fonts through the renderer. Empty paths fall back to the
    // previous font in the chain: title -> body, ui -> title, medium -> ui,
    // mono -> body. Returns false on error. Init must be called before any
    // widget draws.
    bool Init(Renderer& renderer, const std::string& fontPath,
              const std::string& titleFontPath = {}, f32 baseSize = 16.0f,
              f32 titleSize = 30.0f, const std::string& uiFontPath = {},
              f32 uiSize = 15.0f, const std::string& mediumFontPath = {},
              f32 mediumSize = 12.5f,
              const std::string& monoFontPath = {}, f32 monoSize = 13.0f,
              f32 dpiScale = 1.0f);

    // Preferred entry: load the font chain from a Theme::Typography struct so
    // weights, sizes, and family paths are all driven by the design system.
    // Empty string fields fall back to the previous weight in the chain
    // (semiBold -> medium -> regular). `dpiScale` (default 1.0) multiplies
    // every pixel size in `type` before baking the atlases so the product
    // stays crisp on hi-DPI displays without changing the design tokens.
    bool Init(Renderer& renderer, const Typography& type, f32 dpiScale = 1.0f);

    // --- Input feed (call from the app's event callback) --------------------
    void OnMouseMove(f32 x, f32 y);
    void OnMouseButton(int button, bool down);  // 0=left,1=right,2=middle
    void OnScroll(f32 y);
    void OnText(u32 codepoint);
    void OnKey(int glfwKey, bool down);

    // --- Frame --------------------------------------------------------------
    void BeginFrame(f32 displayWidth, f32 displayHeight, f32 dt);
    const UIDrawData& EndFrame();

    // Animates 0..1 toward `active ? 1 : 0` with an exponential lerp at
    // `speed` per second. State is keyed by `id` so multiple independent
    // animations can coexist (hover fades, popup reveals, etc.). Returns
    // the current value. Each frame, ids that haven't been touched in
    // kAnimGcAge frames are evicted from the map.
    f32 Animate(u64 id, bool active, f32 speed);

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
    void PushClip(const Rect& rect);
    void PopClip();
    void ImageUV(TextureHandle texture, const Rect& rect, const Rect& uv,
                 Color tint = Color::RGB(255, 255, 255));
    // Draws the Luma gem/prism logo mark centered at `center`.
    void LogoMark(Vec2 center, f32 radius);
    void Label(Vec2 pos, std::string_view text, Color color);
    void LabelIn(const Rect& rect, std::string_view text, Color color,
                 Align align = Align::Left, bool title = false);
    // Label rendered with the monospace font (console / logs / code).
    void LabelInMono(const Rect& rect, std::string_view text, Color color,
                     Align align = Align::Left);
    // Draws text in the semibold UI font (for tab titles, section headers).
    void Heading(const Rect& rect, std::string_view text, Color color,
                 Align align = Align::Left);
    bool Button(u64 id, const Rect& rect, std::string_view label);
    // A flat menu-bar item: text only, with a rounded highlight on hover/press.
    bool MenuButton(u64 id, const Rect& rect, std::string_view label);
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
    // A numeric field you scrub by dragging horizontally. Returns true if the
    // value changed this frame.
    bool DragFloat(u64 id, const Rect& rect, f32& value, f32 speed = 0.02f);
    // Three drag fields with colored X/Y/Z labels (edits xyz[0..2] in place).
    bool Vector3Field(u64 id, const Rect& rect, f32* xyz);
    // A collapsible section header; toggles `open` when clicked.
    bool CollapsingHeader(u64 id, const Rect& rect, std::string_view label,
                          bool& open);

    // --- New widget catalog ------------------------------------------------
    // Toggle (animated switch). Returns true if value changed.
    bool Toggle(u64 id, const Rect& rect, bool& value);
    // Slider with [min..max] range. Returns true if value changed.
    bool Slider(u64 id, const Rect& rect, f32& value, f32 min, f32 max);
    // Dropdown. Returns the new index (or `current` if unchanged / closed).
    // `items` is a list of labels; `current` is the selected index. The popup
    // opens on click, closes on selection or outside-click.
    int Dropdown(u64 id, const Rect& rect,
                 const std::vector<std::string>& items, int current);
    // Hierarchical tree node. Returns true if the open state toggled.
    bool TreeNode(u64 id, const Rect& rect, std::string_view label, Icon icon,
                  bool& open, int depth = 0, bool selected = false);
    // Inspector-style property row: label column on the left, returns the
    // field rect on the right so the caller can place a control inside it.
    // `labelWidth` is the label column width in px.
    Rect PropertyRow(const Rect& rect, std::string_view label,
                     f32 labelWidth = 120.0f);
    // Input + search icon + clear-X when text is non-empty. Returns true if
    // text changed (typing or clearing).
    bool SearchBox(u64 id, const Rect& rect, std::string& text,
                   std::string_view placeholder = "Search...");
    // Same as above, but uses the supplied texture as the leading icon
    // instead of the procedural Search glyph. Falls back to the glyph
    // when texture == 0.
    bool SearchBox(u64 id, const Rect& rect, std::string& text,
                   TextureHandle leadingIcon,
                   std::string_view placeholder = "Search...");
    // Modal popup menu. Returns the clicked item index, or -1 if dismissed.
    // Closes on selection or outside-click; backdrop dims the rest of the UI.
    struct MenuItem {
        std::string label;
        Icon icon = Icon::None;
        bool enabled = true;
        bool separatorAfter = false;
    };
    int MenuPopup(u64 id, const Rect& anchor,
                  const std::vector<MenuItem>& items);
    // Hover-delay tooltip. Shows `text` after kTooltipDelay frames of hover
    // over `anchor`. Uses elevation shadow.
    void Tooltip(const Rect& anchor, std::string_view text);
    // Modal dialog. BeginModal centers `title`-bar window at given size and
    // dims the rest of the UI; returns the content rect. EndModal closes.
    // ModalButtonRow lays out OK / Cancel at the bottom (returns true for OK).
    struct ModalResult { bool ok = false; bool cancel = false; bool open = true; };
    Rect BeginModal(u64 id, std::string_view title, const Vec2& size);
    void EndModal();
    ModalResult ModalButtonRow(u64 id, const Rect& content,
                               std::string_view okLabel = "OK",
                               std::string_view cancelLabel = "Cancel");
    // Progress bar (determinate). Optional centered text label.
    void ProgressBar(const Rect& rect, f32 fraction, std::string_view text = {});

    // --- Docking primitives -------------------------------------------------
    // Splits `region` by `ratio` (0..1) with a draggable divider. Outputs the
    // two sub-regions; updates `ratio` while dragging. Returns true if dragging.
    bool SplitterV(u64 id, const Rect& region, f32& ratio, Rect& left,
                   Rect& right, f32 thickness = 1.0f);
    bool SplitterH(u64 id, const Rect& region, f32& ratio, Rect& top,
                   Rect& bottom, f32 thickness = 1.0f);
    // Draws a docked panel (background, title bar, border); returns the content
    // rect below the title bar.
    Rect PanelWithTitle(const Rect& rect, std::string_view title);

    // A full-width selectable list row (used by the outliner).
    bool Selectable(u64 id, const Rect& rect, std::string_view label,
                    bool selected);
    // Same as above, but draws an optional leading icon (drawn at left).
    bool Selectable(u64 id, const Rect& rect, std::string_view label,
                    bool selected, Icon icon);

    Theme& theme() { return m_theme; }
    Font& font() { return m_font; }
    Font& titleFont() { return m_titleFont; }
    Font& uiFont() { return m_uiFont; }
    Font& mediumFont() { return m_mediumFont; }
    Font& monoFont() { return m_monoFont; }

    // DPI scale the context was initialized with. Token sizes in widgets
    // multiply by this where crispness matters (icons, control heights,
    // focus rings); it doesn't re-bake fonts (they were baked at scale).
    f32 dpiScale() const { return m_dpiScale; }

    // Underlying draw list. Exposed for advanced widgets and icons that need
    // to emit raw geometry between BeginFrame/EndFrame.
    DrawList& drawList() { return m_draw; }
    const DrawList& drawList() const { return m_draw; }
    Vec2 mouse() const { return m_mouse; }
    Vec2 mouseDelta() const { return m_mouseDelta; }
    f32 scrollDelta() const { return m_scroll; }
    bool isMouseDown(int button) const {
        return button >= 0 && button < 3 ? m_mouseDown[button] : false;
    }

    // Cursor the UI wants this frame (reset to Arrow each BeginFrame); the app
    // applies it via Window::SetCursor after building the UI.
    void RequestCursor(CursorShape shape) { m_requestedCursor = shape; }
    CursorShape RequestedCursor() const { return m_requestedCursor; }
    bool mousePressed(int button) const {
        return button >= 0 && button < 3 ? m_mousePressed[button] : false;
    }
    bool mouseReleased(int button) const {
        return button >= 0 && button < 3 ? m_mouseReleased[button] : false;
    }

    // FNV-1a hash for stable widget ids from string literals.
    static u64 ID(std::string_view s);

private:
    DrawList m_draw;
    Font m_font;
    Font m_titleFont;
    Font m_uiFont;
    Font m_mediumFont;
    Font m_monoFont;
    Theme m_theme = DarkTheme();
    f32 m_dpiScale = 1.0f;

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

    CursorShape m_requestedCursor = CursorShape::Arrow;
    u64 m_hot = 0;
    u64 m_active = 0;
    u64 m_focus = 0;
    usize m_caret = 0;
    f32 m_time = 0.0f;
    f32 m_displayW = 0.0f;
    f32 m_displayH = 0.0f;

    // Popup / modal / tooltip state — only one of each open at a time so
    // outside-clicks dismiss cleanly.
    u64 m_openPopup = 0;       // id of the open MenuPopup / Dropdown
    u64 m_openModal = 0;       // id of the open BeginModal
    u64 m_tooltipAnchor = 0;   // id under tooltip hover-delay
    u32 m_tooltipTouch = 0;    // frame the anchor was last seen
    std::string m_tooltipText; // cached tooltip text
    std::unordered_map<u64, bool> m_modalOk;
    std::unordered_map<u64, bool> m_modalCancel;

    // Per-id animation values (0..1). Each entry tracks the value plus the
    // last frame it was touched so stale entries can be GC'd.
    struct AnimState { f32 value = 0.0f; u32 lastTouch = 0; };
    std::unordered_map<u64, AnimState> m_anim;
    u32 m_frame = 0;
    static constexpr u32 kAnimGcAge = 120;     // evict after ~2s untouched
    static constexpr u32 kTooltipDelay = 30;   // ~0.5s at 60fps
};

}  // namespace Luma::Slate
