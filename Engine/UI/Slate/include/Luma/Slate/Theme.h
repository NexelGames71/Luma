#pragma once

#include <string>

#include "Luma/Slate/Types.h"

// Luma Slate design tokens — the single source of truth for every visual constant
// (color, typography, spacing, sizing, radius, borders, elevation, motion). Widgets
// read from a Theme instead of hardcoding values, so the whole product restyles
// consistently. DarkTheme() builds the default premium/technical dark theme with a
// refined Luma-blue accent.

namespace Luma::Slate {

// --- Color helpers (defined in Theme.cpp) -----------------------------------
Color Lighten(Color c, f32 amount);         // move toward white by amount (0..1)
Color Darken(Color c, f32 amount);          // move toward black by amount (0..1)
Color Mix(Color a, Color b, f32 t);         // linear blend, t in 0..1

// --- Token groups -----------------------------------------------------------
struct Spacing {
    f32 xs = 2.0f, sm = 4.0f, md = 8.0f, lg = 12.0f, xl = 16.0f, xxl = 24.0f;
};
struct Sizing {
    f32 controlSm = 20.0f, controlMd = 24.0f, controlLg = 28.0f;
    f32 iconSm = 14.0f, iconMd = 16.0f, iconLg = 20.0f;
};
struct Radii {
    f32 none = 0.0f, sm = 3.0f, md = 6.0f, lg = 10.0f, pill = 999.0f;
};
struct BorderWidths {
    f32 hairline = 1.0f, thick = 2.0f;
};
struct Motion {
    f32 hover = 14.0f, press = 22.0f, popup = 18.0f;  // per-second lerp factors
};

// Typography scale. Pixel sizes are tuned for a 96 DPI baseline; the renderer
// scales by display DPI at bake time. Weights are Inter-grade names (Regular
// 400, Medium 500, SemiBold 600, Bold 700). Widgets should prefer the named
// roles below (caption/body/heading/title) over hardcoded sizes so the whole
// product stays consistent.
struct Typography {
    // Font family paths (absolute or asset-relative). Empty = fall back to the
    // previous font in the chain.
    std::string uiRegular;     // Inter-Regular.ttf
    std::string uiMedium;      // Inter-Medium.ttf
    std::string uiSemiBold;    // Inter-SemiBold.ttf
    std::string uiBold;        // Inter-Bold.ttf
    std::string mono;          // Consolas / JetBrains Mono etc.

    // Pixel sizes per role.
    f32 captionSize = 12.5f;   // small labels, hint text
    f32 bodySize = 14.0f;      // default UI text, buttons, menus
    f32 bodyStrongSize = 14.0f; // emphasized body (still body-size, heavier weight)
    f32 headingSize = 14.0f;   // panel headers, section titles
    f32 titleSize = 22.0f;     // window / big headers
    f32 displaySize = 32.0f;   // splash, marketing surfaces

    // Line-height factor applied to the baked font line height.
    f32 lineHeightMul = 1.20f;
};

// Soft-shadow presets consumed by DrawList::AddRectShadow (slice 2).
enum class Elevation { None, E1, E2, E3 };

struct Theme {
    // -- Legacy flat fields (kept so existing call sites compile unchanged) --
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
    f32 rounding = 6.0f;  // == radius.md

    // -- Palette: neutral surface ramp (deep -> raised) --
    Color surface0;  // window background
    Color surface1;  // panels
    Color surface2;  // headers / raised
    Color surface3;  // controls / buttons
    Color surface4;  // overlays / menus / popups

    // -- Semantic colors --
    Color separator;      // flush-panel dividers (dark hairline)
    Color outline;        // raised control edge
    Color focusRing;      // focus outline (== accent)
    Color textDisabled;   // disabled text
    Color selectionBg;    // selected row/item fill
    Color selectionText;  // text on selection
    Color accentHover;
    Color accentActive;
    Color accentMuted;    // low-chroma accent (selection surfaces)
    Color success;
    Color warning;
    Color error;

    // -- Scales --
    Spacing space;
    Sizing size;
    Radii radius;
    BorderWidths border;
    Motion motion;
    Typography type;
};

Theme DarkTheme();

}  // namespace Luma::Slate
