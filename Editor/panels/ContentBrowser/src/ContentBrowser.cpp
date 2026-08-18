#include "Luma/Editor/Panels/ContentBrowser.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>

#include "Luma/Slate/Icons.h"
#include "Luma/Asset/ThumbnailRenderer.h"
#include "Luma/Core/Log.h"

namespace Luma::Editor::Panels {

using Slate::Align;
using Slate::Icon;
using Slate::Rect;
using Slate::Vec2;

ContentBrowserPanel::ContentBrowserPanel() = default;

void ContentBrowserPanel::SetIcons(Luma::TextureHandle sortUp,
                                   Luma::TextureHandle sortDown,
                                   Luma::TextureHandle searchGlass,
                                   Luma::TextureHandle folder,
                                   Luma::TextureHandle reload,
                                   Luma::TextureHandle importTex,
                                   Luma::TextureHandle openFolder,
                                   Luma::TextureHandle expandArrow,
                                   Luma::TextureHandle retractArrow) {
    m_texSortUp = sortUp;
    m_texSortDown = sortDown;
    m_texSearchGlass = searchGlass;
    m_texFolder = folder;
    m_texReload = reload;
    m_texImport = importTex;
    m_texOpenFolder = openFolder;
    m_texExpandArrow = expandArrow;
    m_texRetractArrow = retractArrow;
    m_tree.SetFolderTexture(folder);
    m_tree.SetOpenFolderTexture(openFolder);
    m_tree.SetExpandTexture(expandArrow);
    m_tree.SetRetractTexture(retractArrow);
}

void ContentBrowserPanel::SetRenderer(Luma::Renderer* renderer) {
    m_renderer = renderer;
}

void ContentBrowserPanel::ResetNavigation() {
    m_currentFolder.clear();
    m_tree.SetSelectedFolder({});
    m_selected = AssetId{};
}

void ContentBrowserPanel::NavigateTo(const std::filesystem::path& folder) {
    m_currentFolder = folder.lexically_normal();
    m_tree.SetSelectedFolder(m_currentFolder);
}

void ContentBrowserPanel::NavigateUp() {
    if (m_currentFolder.empty()) return;
    auto parent = m_currentFolder.parent_path();
    if (parent == m_currentFolder) return;
    m_currentFolder = parent;
}

std::vector<std::filesystem::path> ContentBrowserPanel::BreadcrumbSegments()
    const {
    std::vector<std::filesystem::path> segs;
    std::filesystem::path acc;
    for (auto part : m_currentFolder) {
        acc /= part;
        segs.push_back(acc);
    }
    return segs;
}

Slate::Icon ContentBrowserPanel::IconForType(AssetType t) noexcept {
    switch (t) {
        case AssetType::Texture: return Icon::Image;
        case AssetType::Mesh: return Icon::Cube;
        case AssetType::Material: return Icon::Sphere;
        case AssetType::Shader: return Icon::Plane;
        case AssetType::Script: return Icon::Cylinder;
        case AssetType::Prefab: return Icon::Cube;
        case AssetType::Scene: return Icon::Plane;
        case AssetType::Sound: return Icon::Play;
        case AssetType::Font: return Icon::Refresh;
        case AssetType::Folder: return Icon::Folder;
        default: return Icon::Dot;
    }
}

std::vector<const AssetData*> ContentBrowserPanel::CurrentEntries() const {
    if (!m_registry) return {};
    std::optional<std::filesystem::path> dir;
    if (!m_currentFolder.empty()) {
        dir = m_currentFolder;
    } else if (!m_registry->Roots().empty()) {
        // When at root, filter by the root directory to show only direct children
        dir = m_registry->Roots()[0];
    }
    return m_registry->Filter(m_typeFilter, dir, m_nameFilter);
}

void ContentBrowserPanel::DrawToolbar(Slate::Context& ui,
                                      const Slate::Rect& rect) {
    Slate::Theme& t = ui.theme();
    ui.GradientRect(rect, t.surface2, t.surface1);
    ui.Panel({rect.x, rect.Bottom() - 1.0f, rect.w, 1.0f}, t.separator);

    // Shared chrome height for all toolbar-row controls (reload, import,
    // search bar).
    constexpr f32 kBarH = 24.0f;

    // Reload button (re-scans the registry). Rendered as a bare icon —
    // no button chrome: draw the PNG directly, 20px inside a 24px hit
    // rect, tinted brighter on hover. Back/forward navigation now lives
    // in the breadcrumb.
    Rect reloadR{rect.x + 8.0f, rect.y + 6.0f, 24.0f, 24.0f};
    {
        bool reloadHover = reloadR.Contains(ui.mouse());
        if (reloadHover) ui.RequestCursor(Luma::CursorShape::Hand);
        if (reloadHover && ui.mousePressed(0)) m_reloadPressed = true;
        bool clicked = false;
        if (ui.mouseReleased(0)) {
            if (m_reloadPressed && reloadHover) clicked = true;
            m_reloadPressed = false;
        }
        constexpr f32 kReloadIcon = 20.0f;
        Rect iconR{reloadR.x + (reloadR.w - kReloadIcon) * 0.5f,
                   reloadR.y + (reloadR.h - kReloadIcon) * 0.5f,
                   kReloadIcon, kReloadIcon};
        if (m_texReload) {
            ui.Image(m_texReload, iconR,
                     reloadHover ? t.text : t.textDim);
        } else {
            Slate::DrawIcon(ui, iconR, Slate::Icon::Refresh,
                            reloadHover ? t.text : t.textDim);
        }
        if (clicked && m_registry) m_registry->Scan();
    }

    // Import button (round-rect pill with "Import" label + a 90-deg-CW
    // rotated orange import icon matching UE theme). Sits between the reload icon and the
    // breadcrumb box. The whole pill highlights on hover and depresses on
    // press, both driven by Animate so the transitions are smooth (not
    // instant).
    const Slate::Color kImportOrange = Slate::Color::RGB(255, 140, 0);
    const Slate::Color kImportRest = t.surface3;
    const Slate::Color kImportHover = kImportOrange;
    constexpr f32 kImportIconSide = 16.0f;
    constexpr f32 kImportPadX = 8.0f;
    const Slate::Font& uiF = ui.uiFont();
    Vec2 importTextSize = uiF.Measure("Import");
    f32 importW = kImportIconSide + 6.0f + importTextSize.x +
                  kImportPadX * 2.0f;
    Rect importR{rect.x + 40.0f, rect.y + 6.0f, importW, kBarH};
    u64 importId = Slate::Context::ID("cb.import");
    {
        bool hover = importR.Contains(ui.mouse());
        if (hover) ui.RequestCursor(Luma::CursorShape::Hand);
        if (hover && ui.mousePressed(0)) m_importPressed = true;
        bool pressed = m_importPressed && hover;
        bool clicked = false;
        if (ui.mouseReleased(0)) {
            if (m_importPressed && hover) clicked = true;
            m_importPressed = false;
        }
        // Smooth hover fade (0..1) + press fade (0..1). Press darkens
        // the fill slightly so it reads as a depression.
        f32 hoverT = ui.Animate(importId ^ 0xBEEFCAFEull, hover,
                                t.motion.hover);
        f32 pressT = ui.Animate(importId ^ 0xFEEDFACEull, pressed,
                                t.motion.press);
        Slate::Color bg = Slate::Mix(kImportRest, kImportHover, hoverT);
        bg = Slate::Darken(bg, 0.10f * pressT);
        // Press kit subtly shrinks the pill inward (a "click" feel).
        f32 shrink = 0.5f * pressT;
        Rect drawR = importR.Inset(shrink, shrink);
        ui.PanelRounded(drawR, bg, t.radius.pill);
        ui.PanelRoundedBordered(drawR, Slate::Mix(t.outline, kImportOrange,
                                                  0.6f * hoverT),
                                t.outline, t.radius.pill,
                                t.border.hairline);

        // Rotated (90deg CW) orange icon on the left of the pill; brighter
        // as hover fades in.
        f32 iconCx = drawR.x + kImportPadX + kImportIconSide * 0.5f;
        f32 iconCy = drawR.y + drawR.h * 0.5f;
        Slate::Color iconColor = Slate::Mix(kImportOrange,
                                            Slate::Lighten(kImportOrange, 0.25f),
                                            hoverT);
        if (m_texImport) {
            ui.drawList().AddImageRotated(
                m_texImport, {iconCx, iconCy}, kImportIconSide * 0.5f,
                1.5707963f,  // pi/2 = 90 deg CW (rotate the quad so the
                             // image reads rotated)
                iconColor);
        }
        // "Import" label to the right of the icon; dark on green-hover,
        // normal text at rest. The label rect spans the full pill height
        // so LabelIn centers the baseline inside the pill (a rect scaled
        // to the text height starts mid-pill and clips the bottom off).
        // Cancel LabelIn's internal space.lg padding so the glyph sits
        // snug against the icon rather than drifting right.
        Slate::Color labelRest = t.text;
        Slate::Color labelHover = Slate::Color::RGB(20, 30, 24);
        constexpr f32 kLabelGap = 4.0f;
        f32 labelX = drawR.x + kImportPadX + kImportIconSide + kLabelGap -
                     t.space.lg;
        ui.LabelIn({labelX, drawR.y, importTextSize.x + t.space.lg * 2.0f,
                    drawR.h},
                   "Import", Slate::Mix(labelRest, labelHover, hoverT),
                   Align::Left);
        (void)clicked;
    }
    constexpr f32 kImportGap = 8.0f;
    f32 contentStartX = importR.Right() + kImportGap;

    // Search bar with embedded filter toggle.
    // Layout: [reload] [Import] [breadcrumb...] [search][toggle]
    // The toggle lives inside the bar's right edge and shows a down chevron
    // when the filter menu is closed / up chevron when open.
    constexpr f32 kToggleW = 26.0f;
    constexpr f32 kSepW = 1.0f;
    constexpr f32 kBarMaxW = 200.0f;
    constexpr f32 kBarGap = 4.0f;
    f32 barRight = rect.Right() - 8.0f;
    f32 toggleX = barRight - kToggleW;
    // Keep the search bar a fixed, Unreal-style width (not the full
    // toolbar); right-align it against the filter toggle.
    f32 barW = std::min(kBarMaxW, toggleX - contentStartX);
    if (barW < 120.0f) barW = 120.0f;
    f32 barX = toggleX - kSepW - barW;
    Rect barR{barX, rect.y + 6.0f, barW, kBarH};
    Rect textR{barR.x, barR.y, barR.w - kToggleW - kSepW, barR.h};
    Rect toggleR{toggleX, barR.y, kToggleW, barR.h};
    Rect sepR{textR.Right(), barR.y, kSepW, barR.h};

    // The search box reuses the available text rect; its field background
    // covers textR, the leading icon sits at the left of textR, and its
    // built-in clear-X sits at the right of textR (just left of the
    // separator) — none of these overlap the filter toggle zone.
    ui.SearchBox(Slate::Context::ID("cb.search"), textR, m_nameFilter,
                 m_texSearchGlass, "Search assets...");

    // Visual separator between the text field and the filter toggle so
    // they read as distinct zones inside the same bar.
    ui.Panel(sepR, t.separator);

    // Filter toggle button — click toggles the type-filter drop-down.
    // Uses procedural chevron icons (crisp at DPI) tinted with textDim at
    // rest, brighter on hover.
    bool toggleHover = toggleR.Contains(ui.mouse());
    if (toggleHover) {
        ui.PanelRoundedBordered(toggleR.Inset(2.0f, 2.0f), t.surface3, t.outline,
                                t.radius.sm, t.border.hairline);
    }
    if (ui.Selectable(Slate::Context::ID("cb.filter.toggle"), toggleR, "",
                      m_filterMenuOpen, Icon::None)) {
        m_filterMenuOpen = !m_filterMenuOpen;
    }
    // Draw the chevron on top (icon-only button).
    Slate::Icon chev = m_filterMenuOpen ? Slate::Icon::ChevronUp
                                       : Slate::Icon::ChevronDown;
    Slate::DrawIcon(ui,
                    {toggleR.x + (kToggleW - 12.0f) * 0.5f,
                     toggleR.y + (kBarH - 12.0f) * 0.5f, 12.0f, 12.0f},
                    chev, toggleHover ? t.text : t.textDim);

    // Breadcrumb: lives in the gap between Refresh and the search bar,
    // same row, wrapped in a rounded "field" box that matches the search
    // bar's look. The box is sized to the breadcrumb content (not the
    // full gap), so every segment + its trailing chevron are visible and
    // the last word is never clipped. No folder icon; no hover/highlight
    // on segments (the text is the click target).
    constexpr f32 kSegH = 20.0f;
    constexpr f32 kSegPadX = 4.0f;
    constexpr f32 kChevW = 16.0f;
    constexpr f32 kBoxPad = 2.0f;   // left/right padding inside the box
    const Slate::Font& f = ui.uiFont();

    // Build the segment list first so we can measure the total content
    // width and size the box to fit it.
    std::filesystem::path rootTarget;
    if (m_registry && !m_registry->Roots().empty())
        rootTarget = m_registry->Roots().front();
    struct Seg { std::string label; std::filesystem::path target; };
    std::vector<Seg> segs;
    // Root segment is the project's content folder (Assets/ at the project
    // root — the registry is rooted there, not in a Content/ subdirectory).
    segs.push_back({"Assets", rootTarget});
    if (!m_currentFolder.empty() && m_registry) {
        std::string rel = m_registry->DisplayPathFor(m_currentFolder);
        const std::string prefix = "Assets/";
        if (rel.size() > prefix.size() && rel.substr(0, prefix.size()) == prefix)
            rel = rel.substr(prefix.size());
        const std::filesystem::path& absRoot = m_registry->Roots().front();
        std::stringstream ss(rel);
        std::string part;
        std::filesystem::path acc;
        while (std::getline(ss, part, '/')) {
            if (part.empty()) continue;
            acc /= part;
            segs.push_back({part, absRoot / acc});
        }
    }

    // Measure content width: root segment + (chevron + segment) for each
    // additional segment, plus a trailing chevron gap pair spacer.
    f32 contentW = kBoxPad;  // left padding
    for (usize i = 0; i < segs.size(); ++i) {
        if (i > 0) contentW += 2.0f + kChevW;
        contentW += f.Measure(segs[i].label).x + kSegPadX;
    }
    contentW += kBoxPad;  // right padding

    // Box: sized to content, capped to the available gap (so a very long
    // path can't push the box past the search bar).
    f32 bLeft = contentStartX;
    f32 bMaxRight = barX - kBarGap;
    f32 boxW = std::min(contentW, bMaxRight - bLeft);
    Rect boxR{bLeft, rect.y + 6.0f, boxW, kBarH};
    // Match the search bar: outer field-border ring + inner field fill,
    // both rounded with the field radius.
    ui.PanelRounded(boxR, t.fieldBorder, t.radius.md);
    ui.PanelRounded(boxR.Inset(t.border.hairline, t.border.hairline),
                    t.fieldBg,
                    std::max(0.0f, t.radius.md - t.border.hairline));

    // Segment text cursor starts inside the box's left padding.
    f32 x = boxR.x + kBoxPad;
    // Segment band vertically centered in the toolbar (matches the old
    // breadcrumb row baseline, not the box height — keeps text sitting
    // where it has always been, not biased by the box).
    f32 yMid = rect.y + (rect.h - kSegH) * 0.5f;

    // Clip the breadcrumb to the box interior so a path that exceeds the
    // capped box can't bleed across the search bar.
    ui.PushClip({boxR.x, boxR.y, boxR.w, boxR.h});

    auto drawChevron = [&](f32 xPos) {
        Slate::DrawIcon(ui,
                        {xPos, yMid + (kSegH - 10.0f) * 0.5f, 10.0f, 10.0f},
                        Icon::ChevronRight, t.textDisabled);
    };
    // No-chrome segment: hit-test inline (no hover/active fill drawn) and
    // lay down plain text. Tints the leaf (current folder) in textDim.
    // Uses Context::Label (direct position) instead of LabelIn because
    // LabelIn always injects m_theme.space.lg (12px) of left padding,
    // which would push each segment's text 12px right of its cursor and
    // clip the right edge inside a box sized exactly to the text.
    auto drawSegment = [&](const char* label,
                           const std::filesystem::path& target, bool isLast) {
        Vec2 ts = f.Measure(label);
        f32 segW = ts.x + kSegPadX;
        Rect r{x, yMid, segW, kSegH};
        bool hover = r.Contains(ui.mouse());
        if (hover) ui.RequestCursor(Luma::CursorShape::Hand);
        if (hover && ui.mousePressed(0)) m_breadPressed = true;
        if (ui.mouseReleased(0)) {
            if (m_breadPressed && hover) NavigateTo(target);
            m_breadPressed = false;
        }
        // Center the text baseline within the segment band (kSegH) and
        // left-align to the cursor (no implicit padding).
        f32 textY = yMid + (kSegH - f.LineHeight()) * 0.5f;
        ui.Label({x, textY}, label, isLast ? t.textDim : t.text);
        x += segW;
    };

    for (usize i = 0; i < segs.size(); ++i) {
        if (i > 0) {
            x += 2.0f;
            drawChevron(x);
            x += kChevW;
        }
        bool isLast = (i + 1 == segs.size());
        drawSegment(segs[i].label.c_str(), segs[i].target, isLast);
    }
    ui.PopClip();

    // Drop-down: deferred to after the panes are drawn (see Draw) so it
    // overlays the tree / grid instead of being painted over by them.
    m_filterAnchor = toggleR;
}

void ContentBrowserPanel::DrawFilterMenu(Slate::Context& ui,
                                        const Slate::Rect& anchor) {
    Slate::Theme& t = ui.theme();

    // Items: All clears the filter; the rest pick a single AssetType. The
    // order mirrors the brief (Mesh / 3D model, Material, Texture, Scene,
    // Shaders, Audios) with All first so it's a one-click reset.
    struct Item {
        const char* label;
        std::optional<AssetType> type;  // nullopt = All
        Slate::Icon icon;
    };
    const Item kItems[] = {
        {"All", std::nullopt, Slate::Icon::Dot},
        {"Mesh (3D model)", AssetType::Mesh, Slate::Icon::Cube},
        {"Material", AssetType::Material, Slate::Icon::Sphere},
        {"Texture", AssetType::Texture, Slate::Icon::Image},
        {"Scene", AssetType::Scene, Slate::Icon::Plane},
        {"Shaders", AssetType::Shader, Slate::Icon::Plane},
        {"Audios", AssetType::Sound, Slate::Icon::Play},
    };

    // Layout: anchor-width menu below the toggle, each row the same height
    // as the search bar so the rows feel like the bar's siblings.
    constexpr f32 kMenuRowH = 24.0f;
    constexpr f32 kPadX = 10.0f;
    const Slate::Font& f = ui.uiFont();
    f32 maxW = anchor.w;
    for (const Item& it : kItems) {
        f32 w = f.Measure(it.label).x + kPadX * 2.0f + 24.0f;  // icon+check
        if (w > maxW) maxW = w;
    }
    f32 menuW = std::max(maxW, anchor.w);
    f32 menuH = kMenuRowH * static_cast<f32>(std::size(kItems));
    Rect menuR{anchor.x + anchor.w - menuW,
               anchor.Bottom() + 2.0f, menuW, menuH};

    // Outside-click dismisses (press outside both the menu and the toggle
    // that opened it).
    if (ui.mousePressed(0) && !menuR.Contains(ui.mouse()) &&
        !anchor.Contains(ui.mouse())) {
        m_filterMenuOpen = false;
    }

    // Popup panel: rounded fill + outline (no shadow — the public Context
    // API doesn't expose the shadow draw call, and the surface contrast
    // against the toolbar is enough separation).
    ui.PanelRoundedBordered(menuR, t.surface4, t.outline, t.radius.md,
                            t.border.hairline);

    for (usize i = 0; i < std::size(kItems); ++i) {
        const Item& it = kItems[i];
        Rect row{menuR.x, menuR.y + kMenuRowH * static_cast<f32>(i),
                 menuR.w, kMenuRowH};
        bool isActive =
            (!m_typeFilter.has_value() && !it.type.has_value()) ||
            (m_typeFilter.has_value() && it.type.has_value() &&
             m_typeFilter.value() == it.type.value());
        // Selectable handles hover fill + click detection. We pass the
        // selected state for the active row's emphasis and draw the
        // checkmark / icon / label on top.
        u64 rowId = Slate::Context::ID("cb.filter.item") ^ static_cast<u64>(i);
        if (ui.Selectable(rowId, row, "", isActive, Slate::Icon::None)) {
            m_typeFilter = it.type;
            m_filterMenuOpen = false;
        }
        // Checkmark (left) for the active row.
        if (isActive) {
            Slate::DrawIcon(ui,
                            {row.x + 8.0f, row.y + (kMenuRowH - 12.0f) * 0.5f,
                             12.0f, 12.0f},
                            Slate::Icon::Check, t.selectionText);
        }
        // Leading icon (asset type glyph).
        Slate::DrawIcon(ui,
                        {row.x + 24.0f, row.y + (kMenuRowH - 14.0f) * 0.5f,
                         14.0f, 14.0f},
                        it.icon, isActive ? t.selectionText : t.text);
        // Label.
        Vec2 ts = f.Measure(it.label);
        ui.LabelIn({row.x + 44.0f, row.y + (kMenuRowH - ts.y) * 0.5f,
                    row.w - 52.0f, kMenuRowH},
                   it.label,
                    isActive ? t.selectionText : t.text,
                     Slate::Align::Left);
    }
}

void ContentBrowserPanel::DrawGridPane(Slate::Context& ui,
                                       const Slate::Rect& rect,
                                       PanelContext& ctx) {
    if (m_renderer == nullptr && ctx.renderer != nullptr) {
        m_renderer = ctx.renderer;
    }

    Slate::Theme& t = ui.theme();
    // Light shadow background: the surrounding content-browser body
    // (t.surface1) shows through, with a subtle dark inset so the grid
    // reads as a softer, recessed area without a hard panel fill.
    ui.Panel(rect, Slate::Darken(t.surface1, 0.10f));

    auto entries = CurrentEntries();
    
    // Diagnostic: confirm DrawGridPane is being called with assets
    static int gridDrawCount = 0;
    if (++gridDrawCount % 60 == 0) {  // Log every 60 frames to avoid spam
        LUMA_LOG_DEBUG("ContentBrowser", "DrawGridPane() called with {} entries, renderer={}", 
                       entries.size(), m_renderer != nullptr);
    }

    if (entries.empty()) {
        ui.LabelIn({rect.x, rect.y + 12.0f, rect.w, 22.0f},
                   "  No assets match the current filter.", t.textDim,
                   Align::Center);
        m_gridScroll = 0.0f;
        return;
    }

    // Sort: folders first, then by name (case-insensitive). The toolbar
    // chevron now drives the filter menu (not sort direction), so the grid
    // always sorts ascending by name.
    std::sort(entries.begin(), entries.end(),
              [](const AssetData* a, const AssetData* b) {
                  if (a->IsFolder() != b->IsFolder())
                      return a->IsFolder();
                  std::string an = a->assetName;
                  std::transform(an.begin(), an.end(), an.begin(),
                                 [](unsigned char c) { return std::tolower(c); });
                  std::string bn = b->assetName;
                  std::transform(bn.begin(), bn.end(), bn.begin(),
                                 [](unsigned char c) { return std::tolower(c); });
                  if (an == bn) return false;
                  return an < bn;
              });

    // Card grid: each card is kTileW x kTileH with kTileGap between them.
    // Compute columns that fit the available width and start rows from
    // the top padding.
    constexpr f32 kPad = 12.0f;
    constexpr f32 kThumbH = 76.0f;      // top thumbnail strip (reduced)
    constexpr f32 kTextRowsH = kTileH - kThumbH;  // name + type rows
    const Slate::Font& f = ui.uiFont();
    f32 availW = rect.w - kPad * 2.0f;
    f32 colW = kTileW + kTileGap;
    int cols = std::max(1, static_cast<int>(availW / colW));
    f32 startX = rect.x + kPad;
    // Cards shift up by the scroll offset so the grid scrolls like the
    // outliner list; the pane is clipped by the dock, and the scrollbar is
    // added after the loop once the row count is known.
    f32 startY = rect.y + kPad - m_gridScroll;

    // Per-card palette: the card has no bg/border at idle (only the icon
    // + text float). On hover, a bg + border + thumbnail frame + shadow
    // fade in together so the card reveals as a self-contained unit.
    const Slate::Color kCardHover = t.surface3;
    const Slate::Color kBorderHover = Slate::Mix(t.outline, t.accent, 0.5f);

    // Clip the cards to the grid pane so scrolled rows can't overlap the
    // toolbar / breadcrumb above the pane.
    ui.PushClip(rect);

    for (usize i = 0; i < entries.size(); ++i) {
        const auto* a = entries[i];
        int row = static_cast<int>(i) / cols;
        int col = static_cast<int>(i) % cols;
        f32 tx = startX + col * colW;
        f32 ty = startY + row * (kTileH + kTileGap);
        Rect card{tx, ty, kTileW, kTileH};

        // Stable id so Animate() can interpolate hover/select per card
        // across frames without resetting.
        u64 cardId = Slate::Context::ID(
            (a->packagePath.string() + "|card").c_str());

        bool hover = card.Contains(ui.mouse());
        if (hover) ui.RequestCursor(Luma::CursorShape::Hand);

        // Smooth transition: hoverT (0..1) fades card chrome in/out.
        // Use a fast custom speed so the bg/border/shadow disappear
        // quickly when the cursor leaves (no lingering highlight).
        f32 hoverT = ui.Animate(cardId ^ 0xC0FFEEull, hover, 36.0f);

        // Click toggles selection: clicking the already-selected asset
        // clears it, clicking another selects it. The highlight is purely
        // hover-driven — nothing persists when the cursor leaves.
        bool selected = (m_selected == a->id);
        if (ui.Selectable(cardId, card, "", false, Icon::None)) {
            if (a->IsFolder()) {
                m_currentFolder = a->packagePath;
            } else {
                m_selected = (selected ? AssetId{} : a->id);
            }
        }
        // Double-click a non-folder asset to activate (open) it — the
        // signal EditorScreen uses to launch the Material Editor for .lmat.
        if (!a->IsFolder() && hover && ui.mouseDoubleClicked(0)) {
            m_activated = a->id;
        }
        (void)selected;  // kept for the toggle logic above

        // Only paint card chrome (bg, border, shadow, thumb frame) when
        // hoverT > 0 for folders; assets always show background.
        bool isFolder = a->IsFolder();
        if (!isFolder || hoverT > 0.0f) {
            f32 shadowAlpha = isFolder ? 0.45f * hoverT : 0.45f;
            ui.drawList().AddRectShadow(card, t.radius.md, shadowAlpha, 3.0f);
            ui.PanelRounded(card, kCardHover, t.radius.md);
            // Orange border only on hover
            if (hoverT > 0.0f) {
                ui.PanelRoundedBordered(card, kCardHover, kBorderHover,
                                        t.radius.md, t.border.hairline);
            } else {
                ui.PanelRoundedBordered(card, kCardHover, t.outline,
                                        t.radius.md, t.border.hairline);
            }
        }

        // Thumbnail strip (top portion of the card). Slightly darker so
        // the icon reads as framed inside the card. Fades in with hover for folders.
        Rect thumb{card.x + 4.0f, card.y + 4.0f, card.w - 8.0f,
                   kThumbH - 4.0f};
        if (!isFolder || hoverT > 0.0f) {
            ui.PanelRounded(thumb, Slate::Darken(kCardHover, 0.30f),
                            t.radius.sm);
        }
        
        // Centered icon or rendered thumbnail in the thumbnail strip.
        u64 thumbTex = 0;
        if (!isFolder) {
            Luma::Renderer* activeRenderer =
                (ctx.renderer != nullptr) ? ctx.renderer : m_renderer;
            thumbTex = GetThumbnailTexture(a->id, a->packagePath, activeRenderer);
        }

        if (thumbTex != 0) {
            // Draw the rendered thumbnail
            Rect thumbImg = thumb.Inset(2.0f, 2.0f);
            ui.Image(thumbTex, thumbImg);
        } else {
            Slate::Icon icon = isFolder ? Icon::Folder
                                         : IconForType(a->type);
            Slate::Color iconColor = isFolder
                                         ? Slate::Color{255, 178, 92, 255}
                                         : t.textDim;
            f32 iconSide = std::min(thumb.w, thumb.h) * 0.62f;
            Rect iconR{thumb.x + (thumb.w - iconSide) * 0.5f,
                       thumb.y + (thumb.h - iconSide) * 0.5f,
                       iconSide, iconSide};
            if (isFolder && m_texOpenFolder) {
                ui.Image(m_texOpenFolder, iconR);
            } else if (isFolder && m_texFolder) {
                ui.Image(m_texFolder, iconR, iconColor);
            } else {
                Slate::DrawIcon(ui, iconR, icon, iconColor);
            }
        }

        // Asset name (1 line, centered). Always visible.
        f32 nameX = card.x + 4.0f;
        f32 nameY = card.y + kThumbH + 2.0f;
        f32 nameW = card.w - 8.0f;
        ui.LabelIn({nameX, nameY, nameW, 14.0f}, a->assetName, t.text,
                   Align::Center);

        // Asset type (smaller, dimmer, centered at bottom). Skipped for folders.
        if (!isFolder) {
            std::string_view typeName = AssetTypeName(a->type);
            f32 typeY = card.y + kTileH - 16.0f;  // Position at bottom of card
            ui.LabelIn({nameX, typeY, nameW, 12.0f}, typeName,
                       t.textDisabled, Align::Center);
        }
    }
    ui.PopClip();
    (void)kTextRowsH;  // layout constant kept for future tweaks
    (void)f;  // future-proofing for ellipsizing

    // Scroll region spans the grid pane (bar starts right under the
    // toolbar). Content height = top padding + rows + bottom padding, so the
    // thumb reaches the end exactly when the last row is visible.
    int rows = (static_cast<int>(entries.size()) + cols - 1) / cols;
    f32 contentH = kPad * 2.0f +
                   static_cast<f32>(rows) * (kTileH + kTileGap);
    m_gridScroll = ui.VerticalScroll(Slate::Context::ID("cb.grid"), rect,
                                     contentH, m_gridScroll);
}

u64 ContentBrowserPanel::GetThumbnailTexture(const AssetId& assetId, const std::filesystem::path& nativePath, Luma::Renderer* renderer) {
    if (!renderer) {
        LUMA_LOG_ERROR("ContentBrowser", "GetThumbnailTexture: renderer is null");
        return 0;
    }

    // Check cache first
    auto it = m_thumbnailCache.find(assetId);
    if (it != m_thumbnailCache.end()) {
        return it->second;  // Return cached result (may be 0 if failed before)
    }
    
    // Request or load thumbnail from thumbnail manager
    auto& thumbnailMgr = ThumbnailManager::Get();
    ThumbnailSettings settings;
    settings.width = 128;
    settings.height = 128;
    settings.autoRegenerate = true;
    
    LUMA_LOG_DEBUG("ContentBrowser", "GetThumbnailTexture() requesting thumbnail for asset {} from {}", 
                   ToString(assetId), nativePath.filename().string());
    
    auto thumbnail = thumbnailMgr.GetThumbnail(assetId, nativePath, settings);
    if (!thumbnail) {
        LUMA_LOG_WARN("ContentBrowser", "Failed to load thumbnail for asset {} from {}", 
                      ToString(assetId), nativePath.filename().string());
        m_thumbnailCache[assetId] = 0;
        return 0;
    }
    
    if (!thumbnail->IsValid()) {
        LUMA_LOG_WARN("ContentBrowser", "Thumbnail data invalid for asset {} (size: {}x{})", 
                      ToString(assetId), thumbnail->width, thumbnail->height);
        m_thumbnailCache[assetId] = 0;
        return 0;
    }
    
    // Upload thumbnail to GPU
    TextureHandle handle = renderer->CreateTexture(thumbnail->width, thumbnail->height, thumbnail->pixels.data());
    if (handle == 0) {
        LUMA_LOG_ERROR("ContentBrowser", "Failed to create GPU texture for thumbnail of asset {} ({}x{})", 
                       ToString(assetId), thumbnail->width, thumbnail->height);
        m_thumbnailCache[assetId] = 0;
        return 0;
    }
    
    // Cache successful result
    m_thumbnailCache[assetId] = handle;
    LUMA_LOG_DEBUG("ContentBrowser", "Cached thumbnail texture for asset {} (handle: {})", 
                   ToString(assetId), handle);
    return handle;
}

void ContentBrowserPanel::RequestThumbnail(const AssetId& assetId, const std::filesystem::path& nativePath) {
    // Request thumbnail from thumbnail manager
    auto& thumbnailMgr = ThumbnailManager::Get();
    thumbnailMgr.RequestThumbnail(assetId, nativePath);
    
    // Cache entry (will be updated when texture is uploaded)
    m_thumbnailCache[assetId] = 0;
}

void ContentBrowserPanel::Draw(Slate::Context& ui, const Slate::Rect& body,
                               PanelContext& ctx) {
    Slate::Theme& t = ui.theme();
    
    // Diagnostic: confirm Draw is being called
    static int drawCount = 0;
    if (++drawCount % 60 == 0) {  // Log every 60 frames to avoid spam
        LUMA_LOG_DEBUG("ContentBrowser", "Draw() called (frame {}), registry={}, renderer={}", 
                       drawCount, m_registry != nullptr, ctx.renderer != nullptr);
    }
    
    if (!m_registry) {
        // Registry not wired yet — show a hint and return.
        ui.Panel(body, t.windowBg);
        ui.LabelIn({body.x, body.y + body.h * 0.5f - 11.0f, body.w, 22.0f},
                   "Content Browser: no asset registry wired.", t.textDim,
                   Align::Center);
        return;
    }
    m_bodyRect = body;
    // Right-click anywhere in the body area opens (or re-anchors) the Create
    // menu at the cursor. Only fires when no other popup is already open.
    if (ui.mousePressed(1) && body.Contains(ui.mouse()) &&
        !m_filterMenuOpen) {
        m_contextMenuOpen = true;
        m_contextMenuOpenedThisFrame = true;
        m_contextMenuPos = ui.mouse();
        m_contextSearch.clear();
        m_contextHover = -1;
        m_contextFocus = -1;
        m_contextSubmenu = -1;
    }
    Rect toolbar{body.x, body.y, body.w, kToolbarH};
    f32 bodyTop = toolbar.Bottom();
    // The content-browser body sits on a slightly lighter surface so the
    // dark tree panel reads as an inset panel "inside" the browser,
    // surrounded by a margin rather than flush with the edges.
    Rect bodyArea{body.x, bodyTop, body.w, body.Bottom() - bodyTop};
    ui.Panel(bodyArea, t.surface1);

    // Vertical splitter between the tree and the grid. The user can drag
    // the divider to resize the tree column (SplitterV clamps to a sane
    // range and sets m_treeRatio). The tree sits with a small margin
    // inside the left half; the grid fills the right half with the same
    // margin.
    Rect treeSplit{}, gridSplit{};
    ui.SplitterV(Slate::Context::ID("cb.splitter"), bodyArea, m_treeRatio,
                 treeSplit, gridSplit, 1.0f);
    constexpr f32 kTreeMargin = 3.0f;
    Rect treePane{treeSplit.x + kTreeMargin, bodyTop + kTreeMargin,
                  treeSplit.w - kTreeMargin * 2.0f,
                  bodyArea.h - kTreeMargin * 2.0f};
    Rect gridPane{gridSplit.x + kTreeMargin, bodyTop,
                  gridSplit.w - kTreeMargin * 2.0f, bodyArea.h};

    DrawToolbar(ui, toolbar);
    // Reusable FileSystemTreePanel handles the dark inset folder list.
    // Keep its selection synced with our navigation state both ways.
    m_tree.SetRegistry(m_registry);
    m_tree.SetSelectedFolder(m_currentFolder);
    m_tree.Draw(ui, treePane);
    if (m_tree.SelectedFolder() != m_currentFolder) {
        m_currentFolder = m_tree.SelectedFolder();
    }
    DrawGridPane(ui, gridPane, ctx);

    // Filter drop-down overlays the body panes — drawn last so the tree /
    // grid (which paint solid backgrounds) don't cover it.
    if (m_filterMenuOpen) {
        DrawFilterMenu(ui, m_filterAnchor);
    }
}

void ContentBrowserPanel::DrawFloatingMenu(Slate::Context& ui,
                                           PanelContext& ctx) {
    if (!m_contextMenuOpen) return;
    DrawCreateMenu(ui, ctx, m_contextMenuOpenedThisFrame);
}

void ContentBrowserPanel::DrawCreateMenu(Slate::Context& ui,
                                         PanelContext& ctx,
                                         bool openedThisFrame) {
    if (!m_contextMenuOpen) return;
    Slate::Theme& t = ui.theme();

    // Unreal Content-Browser menu: search on top, a GET section (import),
    // a CREATE section (folder / material quick creates), then a categorized
    // list whose rows open a submenu beside them on hover — the Material row
    // lists the material kinds the engine can actually produce. Only kinds
    // with a real create path exist today; more (Material Instance, Material
    // Function, ...) slot into the category's items as the engine grows.
    const f32 kMenuW = 200.0f;
    const f32 kSubW = 180.0f;
    const f32 kRowH = t.menu.rowH;
    const f32 kHeaderH = t.menu.headerH;
    const f32 kHeaderGap = t.space.md;
    const f32 pad = 6.0f;

    // --- Menu data ---------------------------------------------------------
    struct GetItem {
        const char* label;
    };
    static const GetItem getItems[] = {{"Import to Current Folder"}};
    constexpr int kGetCount =
        static_cast<int>(sizeof(getItems) / sizeof(getItems[0]));

    struct CreateItem {
        const char* label;
        Slate::Icon icon;
    };
    static const CreateItem createItems[] = {
        {"New Folder", Slate::Icon::Folder},
        {"Material", Slate::Icon::Sphere},
    };
    constexpr int kCreateCount =
        static_cast<int>(sizeof(createItems) / sizeof(createItems[0]));

    struct CatItem {
        const char* label;
        Slate::Icon icon;
    };
    struct Category {
        const char* label;
        Slate::Icon icon;
        const CatItem* items;
        int count;
    };
    static const CatItem matItems[] = {{"Material", Slate::Icon::Sphere}};
    static const Category categories[] = {
        {"Material", Slate::Icon::Sphere, matItems,
         static_cast<int>(sizeof(matItems) / sizeof(matItems[0]))},
    };
    constexpr int kCatCount =
        static_cast<int>(sizeof(categories) / sizeof(categories[0]));

    // --- Search filter (case-insensitive substring on labels) -------------
    auto lower = [](std::string s) {
        for (char& c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    };
    std::string needle = lower(m_contextSearch);
    auto matches = [&needle, &lower](const char* label) {
        if (needle.empty()) return true;
        return lower(std::string(label)).find(needle) != std::string::npos;
    };

    bool getVis[kGetCount] = {};
    int getVisCount = 0;
    for (int i = 0; i < kGetCount; ++i) {
        getVis[i] = matches(getItems[i].label);
        if (getVis[i]) ++getVisCount;
    }
    bool createVis[kCreateCount] = {};
    int createVisCount = 0;
    for (int i = 0; i < kCreateCount; ++i) {
        createVis[i] = matches(createItems[i].label);
        if (createVis[i]) ++createVisCount;
    }
    bool catVis[kCatCount] = {};
    bool itemVis[kCatCount][4] = {};
    int subVis[kCatCount] = {};
    bool anyItemMatch[kCatCount] = {};
    int catVisCount = 0;
    for (int c = 0; c < kCatCount; ++c) {
        const Category& cat = categories[c];
        if (matches(cat.label)) catVis[c] = true;
        for (int i = 0; i < cat.count; ++i) {
            if (matches(cat.items[i].label)) {
                itemVis[c][i] = true;
                anyItemMatch[c] = true;
                ++subVis[c];
                catVis[c] = true;
            }
        }
        if (catVis[c]) ++catVisCount;
    }
    const int visCount = getVisCount + createVisCount + catVisCount;

    // Escape: clear the filter first, then dismiss the menu.
    if (ui.keyEscape()) {
        if (!m_contextSearch.empty()) {
            m_contextSearch.clear();
            m_contextFocus = -1;
        } else {
            m_contextMenuOpen = false;
            m_contextSubmenu = -1;
            m_contextSearch.clear();
            m_contextFocus = -1;
            m_contextHover = -1;
            m_pressedCreateRow = 0;
            return;
        }
    }

    // --- Layout & placement ----------------------------------------------
    f32 contentH = t.menu.searchH + pad;
    if (visCount > 0) {
        if (getVisCount > 0) {
            contentH += t.menu.sectionGap + kHeaderH + kHeaderGap +
                        static_cast<f32>(getVisCount) * kRowH;
        }
        if (createVisCount > 0) {
            contentH += t.menu.sectionGap + kHeaderH + kHeaderGap +
                        static_cast<f32>(createVisCount) * kRowH;
        }
        if (catVisCount > 0) {
            contentH += t.menu.sectionGap +
                        static_cast<f32>(catVisCount) * kRowH;
        }
    } else {
        contentH += kRowH;  // "No matches" row
    }

    f32 mx = m_contextMenuPos.x;
    f32 my = m_contextMenuPos.y;
    // Clamp to the panel body so the menu never overflows it.
    if (mx + kMenuW > m_bodyRect.Right()) mx = m_bodyRect.Right() - kMenuW - 4.0f;
    if (my + contentH > m_bodyRect.Bottom()) my = m_bodyRect.Bottom() - contentH - 4.0f;
    if (mx < m_bodyRect.x) mx = m_bodyRect.x + 4.0f;
    if (my < m_bodyRect.y) my = m_bodyRect.y + 4.0f;
    Rect main{mx - pad, my - pad, kMenuW + pad * 2.0f, contentH + pad * 2.0f};

    // Row references + y positions in draw order (GET, CREATE, categories).
    // Section == 0 (GET index), 1 (CREATE index), 2 (category index).
    struct RowRef {
        int section;
        int idx;
    };
    std::vector<RowRef> topRefs;
    std::vector<f32> topYs;
    {
        f32 ly = my + pad + t.menu.searchH + pad;
        if (getVisCount > 0) {
            ly += t.menu.sectionGap + kHeaderH + kHeaderGap;
            for (int i = 0; i < kGetCount; ++i) {
                if (!getVis[i]) continue;
                topRefs.push_back({0, i});
                topYs.push_back(ly);
                ly += kRowH;
            }
        }
        if (createVisCount > 0) {
            ly += t.menu.sectionGap + kHeaderH + kHeaderGap;
            for (int i = 0; i < kCreateCount; ++i) {
                if (!createVis[i]) continue;
                topRefs.push_back({1, i});
                topYs.push_back(ly);
                ly += kRowH;
            }
        }
        if (catVisCount > 0) {
            ly += t.menu.sectionGap;
            for (int c = 0; c < kCatCount; ++c) {
                if (!catVis[c]) continue;
                topRefs.push_back({2, c});
                topYs.push_back(ly);
                ly += kRowH;
            }
        }
    }
    const int topCount = static_cast<int>(topRefs.size());

    // --- Submenu rect (flush beside the main menu, never over it) ---------
    auto subRectFor = [&](int cat, f32& outSx, f32& outSy,
                          f32& outH) -> bool {
        if (cat < 0 || !catVis[cat] || subVis[cat] == 0) return false;
        f32 rowY = -1.0f;
        for (usize k = 0; k < topRefs.size(); ++k) {
            if (topRefs[k].section == 2 && topRefs[k].idx == cat) {
                rowY = topYs[k];
                break;
            }
        }
        if (rowY < 0.0f) return false;
        // Submenu content: BASIC header + matching items.
        outH = pad * 2.0f + t.menu.sectionGap + kHeaderH + kHeaderGap +
               static_cast<f32>(subVis[cat]) * kRowH;
        outSy = rowY;
        if (outSy + outH > m_bodyRect.Bottom()) {
            outSy = m_bodyRect.Bottom() - outH - 4.0f;
        }
        // Always sit flush beside the main menu — never render over it.
        outSx = mx + kMenuW;
        if (outSx + kSubW > m_bodyRect.Right()) {
            f32 clamped = m_bodyRect.Right() - kSubW - 4.0f;
            if (clamped > outSx) outSx = clamped;
        }
        return true;
    };

    int openCat = m_contextSubmenu;
    if (openCat >= 0 && (!catVis[openCat] || subVis[openCat] == 0)) {
        openCat = -1;
    }
    // Hover opens the hovered category's submenu; moving into the submenu (or
    // an active search) keeps it; hovering dead space closes it.
    int hov = -1;
    for (int k = 0; k < topCount; ++k) {
        if (topRefs[static_cast<usize>(k)].section == 2 &&
            Rect{mx, topYs[static_cast<usize>(k)], kMenuW, kRowH}
                .Contains(ui.mouse())) {
            hov = topRefs[static_cast<usize>(k)].idx;
            break;
        }
    }
    {
        f32 sx, sy, h;
        bool subShown = subRectFor(openCat, sx, sy, h) &&
                        Rect{sx - pad, sy - pad, kSubW + pad * 2.0f,
                             h + pad * 2.0f}
                            .Contains(ui.mouse());
        if (hov >= 0) {
            m_contextSubmenu = subVis[hov] > 0 ? hov : -1;
        } else if (!subShown) {
            if (!needle.empty()) {
                // Search: keep the current submenu, or auto-open the first
                // category with matching items so the hit is visible.
                if (m_contextSubmenu < 0 || !catVis[m_contextSubmenu] ||
                    subVis[m_contextSubmenu] == 0) {
                    m_contextSubmenu = -1;
                    for (int c = 0; c < kCatCount; ++c) {
                        if (catVis[c] && anyItemMatch[c]) {
                            m_contextSubmenu = c;
                            break;
                        }
                    }
                }
            } else {
                m_contextSubmenu = -1;
            }
        }
    }
    openCat = m_contextSubmenu;
    if (openCat >= 0 && (!catVis[openCat] || subVis[openCat] == 0)) {
        openCat = -1;
    }

    f32 subSx = 0.0f, subSy = 0.0f, subH = 0.0f;
    bool subValid = subRectFor(openCat, subSx, subSy, subH);
    Rect sub{0, 0, 0, 0};
    if (subValid) {
        sub = {subSx - pad, subSy - pad, kSubW + pad * 2.0f,
               subH + pad * 2.0f};
    }

    // --- Keyboard navigation ---------------------------------------------
    const int flatCount = topCount + (subValid ? subVis[openCat] : 0);
    if (ui.keyDown() && flatCount > 0) {
        m_contextFocus = m_contextFocus < 0
                             ? 0
                             : std::min(m_contextFocus + 1, flatCount - 1);
    }
    if (ui.keyUp() && flatCount > 0) {
        m_contextFocus = m_contextFocus < 0
                             ? flatCount - 1
                             : std::max(m_contextFocus - 1, 0);
    }
    // Caret keys belong to the search box while it's focused.
    if (!ui.textFieldFocused()) {
        if (ui.keyHome() && flatCount > 0) m_contextFocus = 0;
        if (ui.keyEnd() && flatCount > 0) m_contextFocus = flatCount - 1;
        if (ui.keyRight() && m_contextFocus >= 0 &&
            m_contextFocus < topCount) {
            const RowRef& rr = topRefs[static_cast<usize>(m_contextFocus)];
            if (rr.section == 2 && subVis[rr.idx] > 0) {
                m_contextSubmenu = rr.idx;
            }
        }
        if (ui.keyLeft() && m_contextFocus >= topCount && openCat >= 0) {
            // Close the submenu and return focus to the parent row.
            for (int k = 0; k < topCount; ++k) {
                const RowRef& rr = topRefs[static_cast<usize>(k)];
                if (rr.section == 2 && rr.idx == openCat) {
                    m_contextSubmenu = -1;
                    m_contextFocus = k;
                    break;
                }
            }
        }
    }
    if (m_contextFocus >= flatCount) {
        m_contextFocus = flatCount > 0 ? flatCount - 1 : -1;
    }

    // --- Panel: matches the docked panels' background, 1px outline --------
    ui.PanelRoundedBordered(main, t.panelBg, t.outline, t.radius.md,
                            t.border.hairline);

    // --- Search box -------------------------------------------------------
    Rect searchRect{mx + pad, my + pad, kMenuW - pad * 2.0f, t.menu.searchH};
    const u64 searchId = Slate::Context::ID("cb.create.search");
    bool changed = ui.SearchBox(searchId, searchRect, m_contextSearch,
                                m_texSearchGlass, "Start typing to search");
    if (changed) {
        m_contextHover = -1;
        m_contextFocus = 0;  // re-filter: jump to the first match
    }
    // Auto-focus the search box the frame the menu opens.
    if (openedThisFrame) {
        ui.FocusField(searchId);
    }

    auto closeMenu = [&] {
        m_contextMenuOpen = false;
        m_contextSubmenu = -1;
        m_contextSearch.clear();
        m_contextFocus = -1;
        m_contextHover = -1;
        m_pressedCreateRow = 0;
    };

    // Actions (share the folder-resolution logic with the grid navigation).
    auto targetFolder = [&]() -> std::filesystem::path {
        std::filesystem::path target = m_currentFolder;
        if (target.empty() && m_registry && !m_registry->Roots().empty()) {
            target = m_registry->Roots().front();
        }
        return target;
    };
    auto createFolder = [&] {
        std::filesystem::path folder = targetFolder() / "NewFolder";
        int n = 1;
        std::error_code ec;
        while (std::filesystem::exists(folder, ec)) {
            folder = targetFolder() / ("NewFolder " + std::to_string(n++));
        }
        if (std::filesystem::create_directories(folder, ec) || !ec) {
            if (m_registry) m_registry->Scan();
            NavigateTo(folder);
        }
    };
    auto createMaterial = [&] {
        if (ctx.onCreateMaterial) {
            std::filesystem::path created =
                ctx.onCreateMaterial(targetFolder());
            if (!created.empty() && m_registry) {
                m_registry->Scan();
                // Select + activate the new asset so the Material Editor
                // opens on it (Unreal-style: create opens).
                if (const AssetData* ad = m_registry->LookupByPath(created)) {
                    m_selected = ad->id;
                    m_activated = ad->id;
                }
            }
        }
    };
    auto importInto = [&] {
        if (ctx.onImportAssets) ctx.onImportAssets(targetFolder());
    };

    // --- Section headers + rows ------------------------------------------
    f32 y = my + pad + t.menu.searchH + pad;
    int flatIdx = 0;

    if (getVisCount > 0) {
        y += t.menu.sectionGap;
        ui.MenuSectionHeader({mx, y, kMenuW, kHeaderH}, "GET");
        y += kHeaderH + kHeaderGap;
        for (int i = 0; i < kGetCount; ++i) {
            if (!getVis[i]) continue;
            Rect row{mx, y, kMenuW, kRowH};
            const u64 rowId = Slate::Context::ID("cb.create.get") ^
                              static_cast<u64>(i);
            bool hovered = row.Contains(ui.mouse());
            if (hovered) {
                ui.RequestCursor(Luma::CursorShape::Hand);
                m_contextHover = flatIdx;
            }
            if (hovered && ui.mousePressed(0)) m_pressedCreateRow = rowId;
            bool clicked = false;
            if (m_pressedCreateRow == rowId && ui.mouseReleased(0)) {
                if (hovered) clicked = true;
                m_pressedCreateRow = 0;
            }
            bool focused = flatIdx == m_contextFocus;
            if (focused && ui.enterPressed()) clicked = true;
            bool highlighted = hovered || focused;
            if (highlighted) {
                ui.drawList().AddRectFilledRounded(
                    row, t.menu.highlightFill, t.radius.sm);
                if (focused) {
                    ui.drawList().AddRectFilled(
                        {row.x, row.y + 3.0f, 2.5f, row.h - 6.0f}, t.accent);
                }
            }
            const f32 kIconS = 16.0f;
            Rect iconRect{row.x + t.menu.iconInset,
                          row.y + (row.h - kIconS) * 0.5f, kIconS, kIconS};
            const Slate::Color tint =
                highlighted ? t.menu.highlightText : t.text;
            if (m_texImport) {
                ui.Image(m_texImport, iconRect, tint);
            } else {
                Slate::DrawIcon(ui, iconRect, Icon::Refresh, tint);
            }
            ui.Label({row.x + t.menu.iconInset + kIconS + t.space.sm,
                      row.y + (row.h - ui.font().LineHeight()) * 0.5f},
                     getItems[i].label, tint);
            if (clicked) {
                importInto();
                closeMenu();
                return;
            }
            y += kRowH;
            ++flatIdx;
        }
    }

    if (createVisCount > 0) {
        y += t.menu.sectionGap;
        ui.MenuSectionHeader({mx, y, kMenuW, kHeaderH}, "CREATE");
        y += kHeaderH + kHeaderGap;
        for (int i = 0; i < kCreateCount; ++i) {
            if (!createVis[i]) continue;
            const CreateItem& it = createItems[i];
            Rect row{mx, y, kMenuW, kRowH};
            const u64 rowId = Slate::Context::ID(it.label) ^ 0xC0CE1F5Eull;
            bool hovered = row.Contains(ui.mouse());
            if (hovered) {
                ui.RequestCursor(Luma::CursorShape::Hand);
                m_contextHover = flatIdx;
            }
            if (hovered && ui.mousePressed(0)) m_pressedCreateRow = rowId;
            bool clicked = false;
            if (m_pressedCreateRow == rowId && ui.mouseReleased(0)) {
                if (hovered) clicked = true;
                m_pressedCreateRow = 0;
            }
            bool focused = flatIdx == m_contextFocus;
            if (focused && ui.enterPressed()) clicked = true;
            bool highlighted = hovered || focused;
            if (highlighted) {
                ui.drawList().AddRectFilledRounded(
                    row, t.menu.highlightFill, t.radius.sm);
                if (focused) {
                    ui.drawList().AddRectFilled(
                        {row.x, row.y + 3.0f, 2.5f, row.h - 6.0f}, t.accent);
                }
            }
            const f32 kIconS = 16.0f;
            Rect iconRect{row.x + t.menu.iconInset,
                          row.y + (row.h - kIconS) * 0.5f, kIconS, kIconS};
            const Slate::Color tint =
                highlighted ? t.menu.highlightText : t.text;
            Slate::DrawIcon(ui, iconRect, it.icon, tint);
            ui.Label({row.x + t.menu.iconInset + kIconS + t.space.sm,
                      row.y + (row.h - ui.font().LineHeight()) * 0.5f},
                     it.label, tint);
            if (clicked) {
                if (i == 0) {
                    createFolder();
                } else {
                    createMaterial();
                }
                closeMenu();
                return;
            }
            y += kRowH;
            ++flatIdx;
        }
    }

    if (catVisCount > 0) {
        y += t.menu.sectionGap;
        for (int c = 0; c < kCatCount; ++c) {
            if (!catVis[c]) continue;
            const Category& cat = categories[c];
            Rect row{mx, y, kMenuW, kRowH};
            const u64 rowId = Slate::Context::ID("cb.create.cat") ^
                              static_cast<u64>(c);
            bool hovered = row.Contains(ui.mouse());
            if (hovered) {
                ui.RequestCursor(Luma::CursorShape::Hand);
                m_contextHover = flatIdx;
            }
            if (hovered && ui.mousePressed(0)) m_pressedCreateRow = rowId;
            bool clicked = false;
            if (m_pressedCreateRow == rowId && ui.mouseReleased(0)) {
                if (hovered) clicked = true;
                m_pressedCreateRow = 0;
            }
            bool focused = flatIdx == m_contextFocus;
            if (focused && ui.enterPressed()) clicked = true;
            bool highlighted = hovered || focused || (c == openCat);
            if (highlighted) {
                ui.drawList().AddRectFilledRounded(
                    row, t.menu.highlightFill, t.radius.sm);
                if (focused) {
                    ui.drawList().AddRectFilled(
                        {row.x, row.y + 3.0f, 2.5f, row.h - 6.0f}, t.accent);
                }
            }
            const f32 kIconS = 16.0f;
            Rect iconRect{row.x + t.menu.iconInset,
                          row.y + (row.h - kIconS) * 0.5f, kIconS, kIconS};
            const Slate::Color tint =
                highlighted ? t.menu.highlightText : t.text;
            Slate::DrawIcon(ui, iconRect, cat.icon, tint);
            // Submenu chevron on the row's right edge.
            Slate::DrawIcon(ui, {row.Right() - 22.0f,
                                 row.y + (row.h - 16.0f) * 0.5f, 16.0f,
                                 16.0f},
                            Icon::ChevronRight, tint);
            ui.Label({row.x + t.menu.iconInset + kIconS + t.space.sm,
                      row.y + (row.h - ui.font().LineHeight()) * 0.5f},
                     cat.label, tint);
            if (clicked && subVis[c] > 0) m_contextSubmenu = c;
            y += kRowH;
            ++flatIdx;
        }
    }

    if (visCount == 0) {
        Rect row{mx, y, kMenuW, kRowH};
        ui.Label({mx + t.space.lg,
                  row.y + (row.h - ui.font().LineHeight()) * 0.5f},
                 "No matches", t.textDim);
    }

    // --- Submenu (open category's items) ----------------------------------
    if (subValid) {
        const Category& cat = categories[openCat];
        ui.PanelRoundedBordered(sub, t.panelBg, t.outline, t.radius.md,
                                t.border.hairline);
        f32 y2 = subSy;
        y2 += t.menu.sectionGap;
        ui.MenuSectionHeader({subSx, y2, kSubW, kHeaderH}, "BASIC");
        y2 += kHeaderH + kHeaderGap;
        for (int i = 0; i < cat.count; ++i) {
            if (!itemVis[openCat][i]) continue;
            const CatItem& it = cat.items[i];
            Rect row{subSx, y2, kSubW, kRowH};
            const u64 rowId =
                (Slate::Context::ID(it.label) ^ 0x51AB1Eull) +
                static_cast<u64>(openCat) * 7919ull;
            bool hovered = row.Contains(ui.mouse());
            if (hovered) {
                ui.RequestCursor(Luma::CursorShape::Hand);
                m_contextHover = flatIdx;
            }
            if (hovered && ui.mousePressed(0)) m_pressedCreateRow = rowId;
            bool clicked = false;
            if (m_pressedCreateRow == rowId && ui.mouseReleased(0)) {
                if (hovered) clicked = true;
                m_pressedCreateRow = 0;
            }
            bool focused = flatIdx == m_contextFocus;
            if (focused && ui.enterPressed()) clicked = true;
            bool highlighted = hovered || focused;
            if (highlighted) {
                ui.drawList().AddRectFilledRounded(
                    row, t.menu.highlightFill, t.radius.sm);
                if (focused) {
                    ui.drawList().AddRectFilled(
                        {row.x, row.y + 3.0f, 2.5f, row.h - 6.0f}, t.accent);
                }
            }
            const f32 kIconS = 16.0f;
            Rect iconRect{row.x + t.menu.iconInset,
                          row.y + (row.h - kIconS) * 0.5f, kIconS, kIconS};
            const Slate::Color tint =
                highlighted ? t.menu.highlightText : t.text;
            Slate::DrawIcon(ui, iconRect, it.icon, tint);
            ui.Label({row.x + t.menu.iconInset + kIconS + t.space.sm,
                      row.y + (row.h - ui.font().LineHeight()) * 0.5f},
                     it.label, tint);
            if (clicked) {
                createMaterial();
                closeMenu();
                return;
            }
            y2 += kRowH;
            ++flatIdx;
        }
    }

    // --- Outside-click / Escape closes ------------------------------------
    if (!openedThisFrame && ui.mousePressed(0) &&
        !main.Contains(ui.mouse()) && !sub.Contains(ui.mouse())) {
        closeMenu();
    }
    if (!ui.isMouseDown(0)) m_pressedCreateRow = 0;
    m_contextMenuOpenedThisFrame = false;
}

}  // namespace Luma::Editor::Panels
