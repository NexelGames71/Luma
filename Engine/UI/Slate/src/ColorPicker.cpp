#include "Luma/Slate/ColorPicker.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Luma::Slate {

// --- Layout constants (fixed natural size, caller positions the rect) -------
namespace {
constexpr f32 kPad = 8.0f;       // outer padding
constexpr f32 kSvSize = 128.0f;  // saturation/value square side
constexpr f32 kHueW = 14.0f;     // hue strip width
constexpr f32 kSwatchH = 40.0f;  // preview swatch height
constexpr f32 kRowH = 22.0f;     // spin-box row height
constexpr f32 kGap = 6.0f;       // inter-control gap

Rect SvRect(const Rect& rect) {
    return {rect.x + kPad, rect.y + kPad, kSvSize, kSvSize};
}
Rect HueRect(const Rect& rect) {
    Rect sv = SvRect(rect);
    return {sv.Right() + kGap, sv.y, kHueW, sv.h};
}
// Right column: preview swatch on top, hex field beneath it.
Rect SwatchRect(const Rect& rect) {
    Rect sv = SvRect(rect);
    f32 w = rect.Right() - kPad - (sv.Right() + kGap + kHueW + kGap + kGap);
    return {sv.Right() + kGap + kHueW + kGap + kGap, sv.y,
            w > 0.0f ? w : 0.0f, kSwatchH};
}
Rect HexRect(const Rect& rect) {
    Rect sw = SwatchRect(rect);
    return {sw.x, sw.Bottom() + kGap, sw.w, 24.0f};
}
Rect RgbRowRect(const Rect& rect) {
    Rect sv = SvRect(rect);
    return {rect.x + kPad, sv.Bottom() + 10.0f, rect.w - 2.0f * kPad, kRowH};
}
Rect HsvRowRect(const Rect& rect) {
    Rect rgb = RgbRowRect(rect);
    return {rgb.x, rgb.Bottom() + kGap, rgb.w, kRowH};
}

int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
}  // namespace

ColorPicker::ColorPicker(Color initial) : m_color(initial) {
    RGBToHSV(m_color, m_hue, m_sat, m_val);
    for (int i = 0; i < 3; ++i) {
        m_rgb[i] = FormatRgb(i, m_color);
        m_hsv[i] = FormatHsv(i, m_hue, m_sat, m_val);
    }
    m_hex = FormatHex(m_color);
}

bool ColorPicker::Draw(Context& ui, const Rect& rect) {
    Theme& t = ui.theme();

    // Self-contained card so the widget reads as a control both inline in a
    // panel and inside the popup wrapper.
    ui.PanelRoundedBordered(rect, t.surface1, t.outline, t.radius.md,
                            t.border.hairline);

    // Re-derive HSV only when the color changed externally (SetColor / scene
    // load) and no drag is in flight — dragging to black would otherwise
    // collapse the hue to 0 and make the hue strip jump.
    bool colorDirty = m_color.r != m_hsvSource.r ||
                      m_color.g != m_hsvSource.g ||
                      m_color.b != m_hsvSource.b;
    if (!m_svDragging && !m_hueDragging && colorDirty) {
        RGBToHSV(m_color, m_hue, m_sat, m_val);
        m_hsvSource = m_color;
    }

    bool changed = false;
    changed |= DrawSVSquare(ui, rect);
    changed |= DrawHueStrip(ui, rect);
    DrawSwatch(ui, rect);
    changed |= DrawHexField(ui, rect);
    changed |= DrawSpinRows(ui, rect);

    if (changed && OnColorChanged) OnColorChanged(m_color);
    return changed;
}

bool ColorPicker::DrawSVSquare(Context& ui, const Rect& rect) {
    Theme& t = ui.theme();
    Rect sv = SvRect(rect);

    // Drag ownership mirrors Context::Slider: capture on press inside,
    // track while the button is held, release on mouse-up.
    bool hovered = sv.Contains(ui.mouse());
    if (hovered) ui.RequestCursor(CursorShape::Hand);
    if (hovered && ui.mousePressed(0)) m_svDragging = true;

    bool changed = false;
    if (m_svDragging) {
        if (ui.isMouseDown(0)) {
            f32 s = std::clamp((ui.mouse().x - sv.x) / sv.w, 0.0f, 1.0f);
            f32 v = std::clamp(1.0f - (ui.mouse().y - sv.y) / sv.h, 0.0f,
                               1.0f);
            m_sat = s;
            m_val = v;
            Color c = HSVToRGB(m_hue, m_sat, m_val);
            if (c.r != m_color.r || c.g != m_color.g || c.b != m_color.b) {
                m_color = c;
                m_hsvSource = c;
                changed = true;
            }
        } else {
            m_svDragging = false;
        }
    }

    // S/V field: horizontal white -> hue, then a transparent -> black
    // vertical fade (the UI pass alpha-blends, so the two compose correctly).
    Color hueFull = HSVToRGB(m_hue, 1.0f, 1.0f);
    ui.drawList().AddRectFilledGradientH(sv, Color::RGB(255, 255, 255),
                                         hueFull);
    ui.drawList().AddRectFilledGradient(sv, Color::RGBA(255, 255, 255, 0),
                                        Color::RGB(0, 0, 0));
    ui.drawList().AddRectOutline(sv, t.outline, t.border.hairline);

    // Current (s, v) marker: a ring that contrasts with the value beneath it.
    Vec2 mp{sv.x + m_sat * sv.w, sv.y + (1.0f - m_val) * sv.h};
    Color ring = m_val > 0.6f ? Color::RGB(18, 18, 18)
                              : Color::RGB(240, 240, 240);
    ui.drawList().AddCircleFilled(mp, 7.0f, ring);
    ui.drawList().AddCircleFilled(mp, 4.5f,
                                  Color::RGBA(ring.r, ring.g, ring.b, 70));
    return changed;
}

bool ColorPicker::DrawHueStrip(Context& ui, const Rect& rect) {
    Rect hue = HueRect(rect);

    bool hovered = hue.Contains(ui.mouse());
    if (hovered) ui.RequestCursor(CursorShape::Hand);
    if (hovered && ui.mousePressed(0)) m_hueDragging = true;

    bool changed = false;
    if (m_hueDragging) {
        if (ui.isMouseDown(0)) {
            m_hue = std::clamp((ui.mouse().y - hue.y) / hue.h, 0.0f, 1.0f) *
                    360.0f;
            Color c = HSVToRGB(m_hue, m_sat, m_val);
            if (c.r != m_color.r || c.g != m_color.g || c.b != m_color.b) {
                m_color = c;
                m_hsvSource = c;
                changed = true;
            }
        } else {
            m_hueDragging = false;
        }
    }

    // Rainbow strip: six vertical gradient bands red->...->red.
    const Color stops[7] = {Color::RGB(255, 0, 0),   Color::RGB(255, 255, 0),
                            Color::RGB(0, 255, 0),   Color::RGB(0, 255, 255),
                            Color::RGB(0, 0, 255),   Color::RGB(255, 0, 255),
                            Color::RGB(255, 0, 0)};
    for (int i = 0; i < 6; ++i) {
        Rect band{hue.x, hue.y + hue.h * static_cast<f32>(i) / 6.0f, hue.w,
                  hue.h / 6.0f};
        ui.drawList().AddRectFilledGradient(band, stops[i], stops[i + 1]);
    }

    // Current hue marker: 2px light line with a dark outline.
    f32 hy = hue.y + (m_hue / 360.0f) * hue.h;
    ui.drawList().AddRectFilled({hue.x - 2.0f, hy - 2.0f, hue.w + 4.0f, 4.0f},
                                Color::RGB(15, 15, 15));
    ui.drawList().AddRectFilled({hue.x - 1.0f, hy - 1.0f, hue.w + 2.0f, 2.0f},
                                Color::RGB(240, 240, 240));
    return changed;
}

void ColorPicker::DrawSwatch(Context& ui, const Rect& rect) {
    Theme& t = ui.theme();
    Rect sw = SwatchRect(rect);
    if (sw.w <= 0.0f) return;
    // Fill with the current color, bordered with the outline token.
    ui.drawList().AddRectFilledRounded(sw, t.outline, t.radius.md);
    ui.drawList().AddRectFilledRounded(
        sw.Inset(t.border.hairline, t.border.hairline), m_color,
        std::max(0.0f, t.radius.md - t.border.hairline));
}

// A text field that commits on Enter or outside-click. `commit` parses the
// buffer and applies it to the color; it must also refresh the buffer. While
// editing, the buffer is left alone so typing is never clobbered.
bool ColorPicker::DrawHexField(Context& ui, const Rect& rect) {
    Rect hex = HexRect(rect);
    if (hex.w <= 0.0f) return false;

    bool changed = false;
    if (ui.mousePressed(0)) {
        if (hex.Contains(ui.mouse())) {
            // First click into the field starts a fresh value (TextField
            // appends at the caret, so typing over an existing value would
            // otherwise concatenate). Re-clicking mid-edit repositions the
            // caret and keeps the buffer.
            if (!m_editingHex) m_hex.clear();
            m_editingHex = true;
        } else if (m_editingHex) {
            changed = CommitHex();
            m_editingHex = false;
        }
    }
    if (m_editingHex && ui.enterPressed()) {
        changed |= CommitHex();
        m_editingHex = false;
    }
    if (!m_editingHex) {
        std::string fmt = FormatHex(m_color);
        if (m_hex != fmt) m_hex = fmt;
    }
    ui.TextField(IdBase() ^ 0x3001, hex, m_hex, "RRGGBB");
    return changed;
}

bool ColorPicker::DrawSpinRows(Context& ui, const Rect& rect) {
    Theme& t = ui.theme();
    const Rect rows[2] = {RgbRowRect(rect), HsvRowRect(rect)};
    const char* labels[2][3] = {{"R", "G", "B"}, {"H", "S", "V"}};
    const Color chips[2][3] = {
        {Color::RGB(196, 78, 82), Color::RGB(96, 176, 96),
         Color::RGB(78, 130, 208)},
        {t.surface3, t.surface3, t.surface3},
    };
    const Color chipText[2][3] = {
        {t.accentText, t.accentText, t.accentText},
        {t.textDim, t.textDim, t.textDim},
    };

    bool changed = false;
    for (int row = 0; row < 2; ++row) {
        const Rect& r = rows[row];
        f32 cellW = (r.w - kGap * 2.0f) / 3.0f;
        const f32 labelW = 16.0f;
        for (int i = 0; i < 3; ++i) {
            Rect cell{r.x + static_cast<f32>(i) * (cellW + kGap), r.y, cellW,
                      r.h};
            // Label chip.
            ui.drawList().AddRectFilledRounded(
                {cell.x, cell.y, labelW, cell.h}, chips[row][i],
                t.radius.sm);
            Vec2 ls = ui.font().Measure(labels[row][i]);
            ui.drawList().AddText(
                ui.font(),
                {cell.x + (labelW - ls.x) * 0.5f,
                 cell.y + (cell.h - ui.font().LineHeight()) * 0.5f},
                labels[row][i], chipText[row][i]);

            Rect field{cell.x + labelW + 2.0f, cell.y,
                       cell.w - labelW - 2.0f, cell.h};
            bool& editing =
                row == 0 ? m_editingRgb[i] : m_editingHsv[i];
            std::string& buf = row == 0 ? m_rgb[i] : m_hsv[i];

            if (ui.mousePressed(0)) {
                if (field.Contains(ui.mouse())) {
                    // See DrawHexField: typing replaces the value on first
                    // click, not appends.
                    if (!editing) buf.clear();
                    editing = true;
                } else if (editing) {
                    changed |= row == 0 ? CommitRgb(i, buf)
                                        : CommitHsv(i, buf);
                    editing = false;
                }
            }
            if (editing && ui.enterPressed()) {
                changed |= row == 0 ? CommitRgb(i, buf) : CommitHsv(i, buf);
                editing = false;
            }
            if (!editing) {
                std::string fmt = row == 0 ? FormatRgb(i, m_color)
                                           : FormatHsv(i, m_hue, m_sat, m_val);
                if (buf != fmt) buf = fmt;
            }
            ui.TextField(IdBase() ^ (row == 0 ? 0x2001u : 0x2101u) ^
                             static_cast<u64>(i),
                         field, buf);
        }
    }
    return changed;
}

bool ColorPicker::CommitRgb(int channel, const std::string& text) {
    // Reject empty / non-numeric input by reverting the buffer (an empty
    // field must not zero the channel).
    char* end = nullptr;
    long v = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        m_rgb[channel] = FormatRgb(channel, m_color);
        return false;
    }
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    u8 b = static_cast<u8>(v);
    Color c = m_color;
    if (channel == 0) c.r = b;
    if (channel == 1) c.g = b;
    if (channel == 2) c.b = b;
    if (c.r != m_color.r || c.g != m_color.g || c.b != m_color.b) {
        m_color = c;
        return true;
    }
    // No change (e.g. the buffer held the same number) — refresh the buffer
    // so stray whitespace / formatting is normalized.
    m_rgb[channel] = FormatRgb(channel, m_color);
    return false;
}

bool ColorPicker::CommitHsv(int channel, const std::string& text) {
    char* end = nullptr;
    f32 v = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        // Invalid / empty — revert the buffer for this channel.
        m_hsv[channel] = FormatHsv(channel, m_hue, m_sat, m_val);
        return false;
    }
    if (channel == 0) {
        m_hue = v;
    } else if (channel == 1) {
        m_sat = v;
    } else {
        m_val = v;
    }
    Color c = HSVToRGB(m_hue, m_sat, m_val);
    if (c.r != m_color.r || c.g != m_color.g || c.b != m_color.b) {
        m_color = c;
        m_hsvSource = c;
        return true;
    }
    m_hsv[channel] = FormatHsv(channel, m_hue, m_sat, m_val);
    return false;
}

bool ColorPicker::CommitHex() {
    std::string t = m_hex;
    if (!t.empty() && t[0] == '#') t.erase(0, 1);
    if (t.size() == 6) {
        int d[6];
        bool ok = true;
        for (int i = 0; i < 6; ++i) {
            d[i] = HexVal(t[i]);
            if (d[i] < 0) ok = false;
        }
        if (ok) {
            Color c = Color::RGB(static_cast<u8>(d[0] * 16 + d[1]),
                                 static_cast<u8>(d[2] * 16 + d[3]),
                                 static_cast<u8>(d[4] * 16 + d[5]));
            if (c.r != m_color.r || c.g != m_color.g || c.b != m_color.b) {
                m_color = c;
                m_hsvSource = c;
                m_hex = FormatHex(c);
                return true;
            }
        }
    }
    // Invalid or unchanged — revert the buffer to the current color.
    m_hex = FormatHex(m_color);
    return false;
}

std::string ColorPicker::FormatRgb(int channel, Color c) {
    char buf[8];
    int v = channel == 0 ? c.r : (channel == 1 ? c.g : c.b);
    std::snprintf(buf, sizeof(buf), "%d", v);
    return buf;
}

std::string ColorPicker::FormatHsv(int channel, f32 h, f32 s, f32 v) {
    char buf[16];
    if (channel == 0) {
        std::snprintf(buf, sizeof(buf), "%.0f", h);
    } else if (channel == 1) {
        std::snprintf(buf, sizeof(buf), "%.3f", s);
    } else {
        std::snprintf(buf, sizeof(buf), "%.3f", v);
    }
    return buf;
}

std::string ColorPicker::FormatHex(Color c) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02X%02X%02X", c.r, c.g, c.b);
    return buf;
}

}  // namespace Luma::Slate
