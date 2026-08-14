#include "Luma/Editor/Panels/ContentBrowser.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>

#include "Luma/Slate/Icons.h"

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
                                   Luma::TextureHandle openFolder) {
    m_texSortUp = sortUp;
    m_texSortDown = sortDown;
    m_texSearchGlass = searchGlass;
    m_texFolder = folder;
    m_texReload = reload;
    m_texImport = importTex;
    m_texOpenFolder = openFolder;
    m_tree.SetFolderTexture(folder);
    m_tree.SetOpenFolderTexture(openFolder);
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
    if (!m_currentFolder.empty()) dir = m_currentFolder;
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
    // rotated green import icon). Sits between the reload icon and the
    // breadcrumb box. The whole pill highlights on hover and depresses on
    // press, both driven by Animate so the transitions are smooth (not
    // instant).
    const Slate::Color kImportGreen = Slate::Color::RGB(76, 180, 120);
    const Slate::Color kImportRest = t.surface3;
    const Slate::Color kImportHover = kImportGreen;
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
        ui.PanelRoundedBordered(drawR, Slate::Mix(t.outline, kImportGreen,
                                                  0.6f * hoverT),
                                t.outline, t.radius.pill,
                                t.border.hairline);

        // Rotated (90deg CW) green icon on the left of the pill; brighter
        // as hover fades in.
        f32 iconCx = drawR.x + kImportPadX + kImportIconSide * 0.5f;
        f32 iconCy = drawR.y + drawR.h * 0.5f;
        Slate::Color iconColor = Slate::Mix(kImportGreen,
                                            Slate::Lighten(kImportGreen, 0.25f),
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
    segs.push_back({"Content", rootTarget});
    if (!m_currentFolder.empty() && m_registry) {
        std::string rel = m_registry->DisplayPathFor(m_currentFolder);
        const std::string prefix = "Content/";
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
                                       PanelContext& /*ctx*/) {
    Slate::Theme& t = ui.theme();
    ui.Panel(rect, t.windowBg);

    auto entries = CurrentEntries();

    if (entries.empty()) {
        ui.LabelIn({rect.x, rect.y + 12.0f, rect.w, 22.0f},
                   "  No assets match the current filter.", t.textDim,
                   Align::Center);
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

    // Tile grid: compute how many columns fit, then draw rows of tiles.
    f32 pad = 12.0f;
    f32 availW = rect.w - pad * 2.0f;
    f32 colW = kTileSize + kTileGap;
    int cols = std::max(1, static_cast<int>(availW / colW));
    f32 startX = rect.x + pad;
    f32 startY = rect.y + pad;

    // Orange tint for folder glyphs.
    const Slate::Color kFolderOrange{255, 178, 92, 255};

    for (usize i = 0; i < entries.size(); ++i) {
        const auto* a = entries[i];
        int row = static_cast<int>(i) / cols;
        int col = static_cast<int>(i) % cols;
        f32 tx = startX + col * colW;
        f32 ty = startY + row * (kTileSize + kTileGap + 18.0f);
        Rect tile{tx, ty, kTileSize, kTileSize};
        Rect labelR{tx - 4.0f, ty + kTileSize + 2.0f, kTileSize + 8.0f,
                    16.0f};

        bool selected = (m_selected == a->id);
        if (ui.Selectable(
                Slate::Context::ID((a->packagePath.string() + "|tile")
                                           .c_str()),
                tile, "", selected)) {
            if (a->IsFolder()) {
                m_currentFolder = a->packagePath;
            } else {
                m_selected = a->id;
            }
        }
        // Tile background + glyph.
        ui.PanelRounded(tile, selected ? t.accentMuted : t.surface2,
                        t.radius.md);
        ui.PanelRoundedBordered(tile, t.outline, t.outline, t.radius.md,
                                t.border.hairline);
        Rect glyphR{tile.x + 12.0f, tile.y + 12.0f, tile.w - 24.0f,
                    tile.h - 24.0f};
        if (a->IsFolder()) {
            // Folder tile: use the supplied orange-tinted folder PNG when
            // available; fall back to the procedural Icon::Folder glyph.
            if (m_texFolder) {
                ui.Image(m_texFolder, glyphR, kFolderOrange);
            } else {
                Slate::DrawIcon(ui, glyphR, Icon::Folder, kFolderOrange);
            }
        } else {
            Slate::DrawIcon(ui, glyphR, IconForType(a->type), t.textDim);
        }

        ui.LabelIn(labelR, a->assetName, t.text, Align::Center);
    }
}

void ContentBrowserPanel::Draw(Slate::Context& ui, const Slate::Rect& body,
                               PanelContext& ctx) {
    Slate::Theme& t = ui.theme();
    if (!m_registry) {
        // Registry not wired yet — show a hint and return.
        ui.Panel(body, t.windowBg);
        ui.LabelIn({body.x, body.y + body.h * 0.5f - 11.0f, body.w, 22.0f},
                   "Content Browser: no asset registry wired.", t.textDim,
                   Align::Center);
        return;
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

}  // namespace Luma::Editor::Panels
