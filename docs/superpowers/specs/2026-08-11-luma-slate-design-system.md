# Luma Slate — Design System & Component Upgrade (Spec)

**Date:** 2026-08-11
**Status:** Proposed (awaiting review)
**Scope:** "Everything at once" — centralized design tokens + all listed components +
shadows/elevation + animation + DPI + procedural icons, rolled out in buildable slices.

## Goal

Evolve Luma Slate from a functional-but-low-fidelity immediate-mode UI into a
coherent, professional design system with a single source of truth for all visual
constants, consumed by every widget. Premium, technical, minimal, high-density,
dark identity with a refined Luma-blue accent. Verified with real editor
screenshots.

## Principles & preservation constraints

- **Improve, don't replace.** Slate stays immediate-mode; widgets remain methods on
  `Slate::Context`. The named components (`LumaButton`, …) map to existing/added
  `Context` methods — **no parallel `Luma*` class layer** (no duplicate systems).
- **Always buildable.** Rolled out in slices; the engine + editor compile and the
  test suite stays green after each slice. Each slice is screenshot-verified.
- **Non-breaking token migration.** The existing `Theme` struct keeps every current
  field (`accent`, `text`, `panelBg`, `rounding`, …) so all current editor / project
  browser call sites keep compiling; new grouped tokens are **added** alongside.
- **Preserve layout.** Type sizes and control heights stay close to current values so
  the editor's existing layout math does not break.

## Non-goals (this iteration)

- Dynamic per-monitor DPI changes at runtime (startup DPI scale only).
- A visual theme editor UI, light theme, or full accessibility pass (tokens are
  structured to allow these later).
- Font hot-reload.

## 1. Token system

New header `Engine/UI/Slate/include/Luma/Slate/Theme.h` (moved out of `Context.h`,
which will `#include` it). `Theme` becomes the token bundle. Existing flat color
fields are retained; new grouped structs are added. `DarkTheme()` populates all.

### 1.1 Palette (raw ramps)
Neutral surface ramp (dark):
- `surface0` window bg `rgb(16,17,21)`
- `surface1` panel `rgb(24,26,32)`
- `surface2` raised / header `rgb(33,36,44)`
- `surface3` control / button `rgb(45,49,59)`
- `surface4` overlay / menu / popup `rgb(52,57,68)`

Accent ramp (refined Luma blue):
- `accent` `rgb(64,142,240)`
- `accentHover` `rgb(88,162,255)`
- `accentActive` `rgb(46,116,208)`
- `accentMuted` (selection fill) `rgb(31,60,100)`

Semantic: `success rgb(76,180,120)`, `warning rgb(226,170,66)`,
`error rgb(226,98,98)`, `info = accent`.

### 1.2 Colors (semantic; existing names kept, remapped to the ramp)
`windowBg=surface0`, `panelBg=surface1`, `header=surface2`, `button=surface3`,
`buttonHover=surface4`, `buttonActive=rgb(38,41,50)`, overlay/menu bg `=surface4`.
Borders: `separator rgb(11,12,15)` (flush-panel dividers, = current panelBorder),
`outline rgb(58,63,74)` (raised control edge), `focusRing = accent`.
Text: `text rgb(234,237,244)`, `textDim rgb(150,158,172)`,
`textDisabled rgb(96,102,114)`, `accentText rgb(247,250,255)`.
Fields: `fieldBg rgb(19,21,26)`, `fieldBorder rgb(56,61,72)`.
Selection: `selectionBg = accentMuted`, `selectionText = accentText`.

### 1.3 Typography (Inter)
Fonts loaded from repo `Inter/` (Regular/Medium/SemiBold/Bold) + a monospace
(Consolas fallback) for console/logs. Roles → {font weight, px size}:
- `caption` Medium 12.5
- `body` Regular 15
- `bodyStrong` SemiBold 15
- `heading` SemiBold 15 (section/tab headers)
- `title` Bold 28 (splash/large)
- `mono` Regular 13

`Context` loads Inter into its `m_font` (Regular), `m_uiFont` (SemiBold),
`m_titleFont` (Bold); a new `m_monoFont` is added. Sizes ×`dpiScale`.

### 1.4 Spacing / Sizing / Radius / Borders
- Spacing scale (px, ×scale): `xs 2, sm 4, md 8, lg 12, xl 16, xxl 24`.
- Sizing: control heights `sm 20, md 24, lg 28`; icon `sm 14, md 16, lg 20`.
- Radius: `none 0, sm 3, md 6, lg 10, pill 999`.
- Border widths: `hairline 1, thick 2`. `rounding` (legacy) = `md`.

### 1.5 Elevation (soft shadow presets)
`e0 none`; `e1` raised (panel/card): offset y1, 3 layers, max alpha ~26;
`e2` popup/menu/dropdown: offset y3, spread, max alpha ~34;
`e3` dialog/modal: offset y8, wider spread, max alpha ~48. Restrained — no large
blooms.

### 1.6 Motion
`hoverSpeed 14`, `pressSpeed 22`, `popupSpeed 18` (per-second exponential lerp
factors). Used only for subtle fades/reveals.

### 1.7 Interaction-state helpers
Free helpers in `Theme.h`: `Lighten(Color, f32)`, `Darken(Color, f32)`,
`Mix(a,b,t)`, `WithAlpha`. Plus `Color StateFill(base, hover, active, disabled)` and
a focus-ring convention (accent 1–2px outline, never a fill) so every widget derives
states identically.

## 2. Rendering primitives (`DrawList`)

Add:
- `AddLine(Vec2 a, Vec2 b, f32 thickness, Color)` — quad-based; enables icons.
- `AddCircleFilled(Vec2 c, f32 r, Color, int segments=16)` — via `AddConvexPolyFilled`.
- `AddRectShadow(const Rect&, f32 radius, Elevation)` — N layered translucent rounded
  rects growing outward with falling alpha.

## 3. Icons (procedural, themeable, DPI-crisp)

`Luma/Slate/Icons.h`: `enum class Icon { None, ChevronRight, ChevronDown, ChevronUp,
Search, Gear, Folder, FolderOpen, Eye, EyeOff, Lock, Plus, Close, Check, Save, Play,
Pause, Stop, Grip, Dot, Trash, Camera, Light, Cube, Sphere, Plane }`.
`Context::DrawIcon(const Rect& box, Icon, Color)` draws with lines/tris/circles.
Existing PNG toolbar icons (play/pause/stop) may be swapped for vector equivalents
(keeps assets optional).

## 4. DPI scaling

`Context` gains `f32 m_dpiScale = 1.0f` and `Init(..., f32 dpiScale)`. All token px
read through accessors that multiply by scale; fonts loaded at `size×scale`.
`main.cpp` queries the primary monitor content scale (GLFW) and passes it. Default 1.0
→ no change on standard displays. `Window` exposes `ContentScale()`.

## 5. Animation

`Context` holds `std::unordered_map<u64,f32> m_anim` (widget id → 0..1), advanced each
frame toward a per-widget target via `1 - exp(-speed*dt)`. Helper
`f32 Context::Animate(u64 id, bool active, f32 speed)`. Used for hover/press fades and
popup reveal. Cleared entries GC'd when untouched for a frame budget.

## 6. Component catalog

All token-driven, with **hover / active / focus / disabled / selected** states as
applicable. Restyled existing + new.

### Restyled (existing `Context` methods)
`Button, IconButton, TextField (LumaInput), Checkbox, Tab, Panel, PanelWithTitle,
Selectable, DragFloat, Vector3Field, CollapsingHeader, SplitterV/H, Card, MenuButton`.

### New `Context` methods
- `bool Toggle(id, rect, bool& value)` — animated switch.
- `bool Slider(id, rect, f32& value, f32 min, f32 max)`.
- `int Dropdown(id, rect, span<const std::string> items, int current)` — closed
  control + popup list; returns new index (or current).
- Tree: `bool TreeNode(id, rect, label, Icon, bool& open, int depth, bool selected)`.
- `Rect PropertyRow(rect, label)` — label column + returns the field rect (Inspector
  building block, `LumaProperty`).
- `bool SearchBox(id, rect, std::string& text, placeholder)` — input + search icon +
  clear affordance.
- Menu: `int MenuPopup(id, Vec2 at, span<const MenuItemDesc>)` — returns clicked item
  index or -1; `MenuItemDesc{label, icon, enabled, separatorAfter}`. Menu bar uses
  `MenuButton` + `MenuPopup`; `ContextMenu` = `MenuPopup` at mouse on right-click.
- `bool Tooltip(rect, text)` — hover-delay tooltip using elevation `e2`.
- Dialog: `Rect BeginModal(id, title, size)` / `void EndModal()` — dims background
  (scrim), centers an `e3` panel, returns content rect; plus `bool ModalButtonRow(...)`.
- `void ProgressBar(rect, f32 fraction, text={})`.

## 7. Module / file changes

- New: `Slate/include/Luma/Slate/Theme.h`, `Slate/include/Luma/Slate/Icons.h`,
  `Slate/src/Theme.cpp` (DarkTheme + helpers), `Slate/src/Icons.cpp` (DrawIcon).
- Edit: `Context.h/.cpp` (tokens include, mono font, dpi, animation, new widgets,
  restyle), `DrawList.h/.cpp` (line/circle/shadow), `Font.*` if needed for scale,
  `Platform/Window` (+`ContentScale`), `Editor/LumaEditor/main.cpp` (Inter + DPI +
  mono), `EditorScreen.cpp` & `ProjectBrowser.cpp` (adopt tokens/components, retire
  hardcoded values), `Slate/CMakeLists.txt` (new sources).
- Tests: `Tests/Slate/` — token invariants (ramp ordering, contrast sanity), color
  helpers (Lighten/Darken/Mix), and `Animate` convergence. (Rendering verified by
  screenshot, not unit tests.)

## 8. Rollout slices (each: build + full tests green + editor screenshot)

1. **Tokens + Inter + restyle existing widgets.** Add `Theme.h`, remap `DarkTheme`,
   load Inter + mono, refactor existing widgets to read tokens. Screenshot editor +
   project browser.
2. **Primitives + DPI + motion.** `AddLine/AddCircleFilled/AddRectShadow`, icons,
   `dpiScale`, `Animate`; apply subtle shadows to panels/cards/popups and hover fades.
   Screenshot.
3. **New widgets.** Toggle, Slider, Dropdown, Tree, PropertyRow, SearchBox, Menu/
   ContextMenu, Tooltip, Dialog, ProgressBar. Screenshot a demo/inspector using them.
4. **Adopt across app.** Real File menu via MenuPopup; Inspector via PropertyRow;
   Outliner via TreeNode; project browser search via SearchBox; retire remaining
   hardcoded literals. Screenshot final editor + browser.

## 9. Verification

- `LumaTests` green after every slice (adds `Tests/Slate/*`).
- Whole engine builds after every slice.
- Editor launched in `--screenshot` mode against a sample project each slice; the PNG
  reviewed for fidelity (typography sharpness, spacing, states, hierarchy).

## 10. Risks & mitigations

- **Layout drift from font swap (Open Sans → Inter).** Mitigate: keep sizes near
  current; screenshot slice 1 immediately; adjust control heights via sizing tokens.
- **Scope size.** Mitigate: strict slice order; each slice independently shippable.
- **DPI regressions on 1.0 displays.** Mitigate: default scale 1.0, values unchanged
  when scale==1.
- **Immediate-mode popups (menu/dropdown/dialog) need retained open-state.** Mitigate:
  store minimal open-id state in `Context` (like existing `m_focus`), one popup at a
  time.

## 11. GOAL.md mapping

Advances Phase 4 (Slate): 4.4 Inter, 4.5 Widgets, 4.6 Layout, 4.8 Theme/tokens,
4.9 Icons, plus motion/elevation and DPI (4.3). Updates those statuses on completion.
