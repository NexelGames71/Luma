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
                                   Luma::TextureHandle folder) {
    m_texSortUp = sortUp;
    m_texSortDown = sortDown;
    m_texSearchGlass = searchGlass;
    m_texFolder = folder;
}

void ContentBrowserPanel::ResetNavigation() {
    m_currentFolder.clear();
    m_selected = AssetId{};
}

void ContentBrowserPanel::NavigateTo(const std::filesystem::path& folder) {
    m_currentFolder = folder.lexically_normal();
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

    // Up button (disabled when at root).
    bool atRoot = m_currentFolder.empty();
    Rect upR{rect.x + 8.0f, rect.y + 6.0f, 24.0f, 24.0f};
    if (!atRoot && ui.Button(Slate::Context::ID("cb.up"), upR, "Up")) {
        NavigateUp();
    }

    // Refresh button.
    Rect refreshR{rect.x + 36.0f, rect.y + 6.0f, 24.0f, 24.0f};
    if (ui.Button(Slate::Context::ID("cb.refresh"), refreshR, "R")) {
        if (m_registry) m_registry->Scan();
    }

    // Search bar with embedded filter toggle.
    // Layout: [search-glass | text-field (with clear-X) || toggle ]
    // The toggle lives inside the bar's right edge and shows a down chevron
    // when the filter menu is closed / up chevron when open.
    constexpr f32 kBarH = 24.0f;
    constexpr f32 kToggleW = 26.0f;
    constexpr f32 kSepW = 1.0f;
    constexpr f32 kGap = 8.0f;
    f32 barRight = rect.Right() - 8.0f;
    f32 toggleX = barRight - kToggleW;
    f32 barW = std::max(160.0f, toggleX - (rect.x + 68.0f) - kGap);
    if (barW < 140.0f) barW = 140.0f;
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

    // Drop-down: drawn after the toolbar so it sits on top of the body.
    if (m_filterMenuOpen) {
        DrawFilterMenu(ui, toggleR);
    }
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

void ContentBrowserPanel::DrawBreadcrumb(Slate::Context& ui,
                                         const Slate::Rect& rect) {
    Slate::Theme& t = ui.theme();
    ui.Panel(rect, t.surface1);
    ui.Panel({rect.x, rect.Bottom() - 1.0f, rect.w, 1.0f}, t.separator);

    // Unreal-style breadcrumb:
    //   [folder] Content  >  SubFolder  >  Leaf
    // Each segment is a clickable button (returns to that folder); the
    // final segment is the current folder and is rendered in textDim to
    // read as the trail's terminus.
    constexpr f32 kSegH = 20.0f;
    constexpr f32 kSegPadX = 8.0f;
    constexpr f32 kIconSize = 14.0f;
    constexpr f32 kIconGap = 4.0f;
    constexpr f32 kSepW = 16.0f;
    const Slate::Color kFolderOrange{255, 178, 92, 255};
    const Slate::Font& f = ui.uiFont();
    f32 x = rect.x + 10.0f;
    f32 yMid = rect.y + (rect.h - kSegH) * 0.5f;

    auto drawFolder = [&](f32 xPos) {
        if (m_texFolder) {
            ui.Image(m_texFolder,
                     {xPos, yMid + (kSegH - kIconSize) * 0.5f, kIconSize,
                      kIconSize},
                     kFolderOrange);
        } else {
            Slate::DrawIcon(ui,
                            {xPos, yMid + (kSegH - kIconSize) * 0.5f,
                             kIconSize, kIconSize},
                            Icon::Folder, kFolderOrange);
        }
    };
    auto drawChevron = [&](f32 xPos) {
        Slate::DrawIcon(ui,
                        {xPos, yMid + (kSegH - 10.0f) * 0.5f, 10.0f, 10.0f},
                        Icon::ChevronRight, t.textDisabled);
    };
    auto drawSegment = [&](u64 id, const char* label,
                           const std::filesystem::path& target,
                           bool isLast) {
        Vec2 ts = f.Measure(label);
        f32 segW = ts.x + kSegPadX * 2.0f;
        Rect r{x, yMid, segW, kSegH};
        bool clicked = ui.Button(id, r, "");
        if (clicked) NavigateTo(target);
        ui.LabelIn({r.x + kSegPadX, r.y + (r.h - ts.y) * 0.5f, r.w - kSegPadX,
                    r.h},
                   label, isLast ? t.textDim : t.text, Align::Left);
        x += segW;
    };

    // Root segment ("Content"): folder icon + clickable label. The
    // target is the empty path (root view) — pick the first registered
    // root to keep the absolute path consistent with the tree pane.
    drawFolder(x);
    x += kIconSize + kIconGap;
    std::filesystem::path rootTarget;
    if (m_registry && !m_registry->Roots().empty())
        rootTarget = m_registry->Roots().front();
    drawSegment(Slate::Context::ID("cb.bread.root"), "Content", rootTarget,
                m_currentFolder.empty());

    if (!m_currentFolder.empty() && m_registry) {
        // Build the relative path ("Foo/Bar") so each segment's label is
        // its leaf name and its click target is the absolute folder.
        std::string rel = m_registry->DisplayPathFor(m_currentFolder);
        const std::string prefix = "Content/";
        if (rel.size() > prefix.size() && rel.substr(0, prefix.size()) == prefix)
            rel = rel.substr(prefix.size());
        const std::filesystem::path& absRoot = m_registry->Roots().front();
        std::stringstream ss(rel);
        std::string part;
        std::filesystem::path acc;
        std::vector<std::pair<std::string, std::filesystem::path>> segs;
        while (std::getline(ss, part, '/')) {
            if (part.empty()) continue;
            acc /= part;
            segs.push_back({part, absRoot / acc});
        }
        for (usize i = 0; i < segs.size(); ++i) {
            x += 2.0f;
            drawChevron(x);
            x += kSepW;
            bool isLast = (i + 1 == segs.size());
            drawSegment(Slate::Context::ID(segs[i].second.string().c_str()),
                        segs[i].first.c_str(), segs[i].second, isLast);
        }
    }
}

void ContentBrowserPanel::DrawTreePane(Slate::Context& ui,
                                       const Slate::Rect& rect,
                                       PanelContext& /*ctx*/) {
    Slate::Theme& t = ui.theme();
    ui.Panel(rect, t.surface0);
    ui.Panel({rect.Right() - 1.0f, rect.y, 1.0f, rect.h}, t.separator);

    // Header.
    ui.Heading({rect.x + 12.0f, rect.y + 8.0f, rect.w - 24.0f, 22.0f},
               "Folders", t.text);

    // Tree rows: for each root, recursively expand one level. Keeps the
    // implementation small; full recursive expand-on-click is a future
    // enhancement. Folder icons use the supplied folder PNG tinted
    // orange; fall back to the procedural Icon::Folder glyph when the
    // texture didn't load.
    f32 y = rect.y + 36.0f;
    if (!m_registry) return;
    // Orange tint applied to the white folder PNG.
    const Slate::Color kFolderOrange{255, 178, 92, 255};
    constexpr f32 kIconSize = 16.0f;
    auto drawFolderIcon = [&](f32 x, f32 y) {
        Rect ir{x, y + (kRowH - kIconSize) * 0.5f, kIconSize, kIconSize};
        if (m_texFolder) {
            ui.Image(m_texFolder, ir, kFolderOrange);
        } else {
            Slate::DrawIcon(ui, ir, Icon::Folder, kFolderOrange);
        }
    };
    for (const auto& root : m_registry->Roots()) {
        // Display label for the root is "Content" (the conventional name of
        // the registered root). Filename is used as a fallback when the
        // registry was wired with a non-Content root.
        std::string name = root.filename().string();
        if (name.empty() || name == "Content") name = "Content";
        Rect r{rect.x + 8.0f, y, rect.w - 16.0f, kRowH};
        bool sel = m_currentFolder.empty() || m_currentFolder == root;
        if (ui.Selectable(Slate::Context::ID(root.string().c_str()), r,
                          name, sel)) {
            m_currentFolder = root;
        }
        drawFolderIcon(r.x + 4.0f, r.y);
        y += kRowH + 2.0f;
        // Show direct child folders of this root.
        if (sel) {
            auto children = m_registry->FilterByDirectory(root);
            for (const auto* child : children) {
                if (!child->IsFolder()) continue;
                Rect cr{rect.x + 24.0f, y, rect.w - 32.0f, kRowH};
                bool csel = (m_currentFolder == child->packagePath);
                if (ui.Selectable(
                        Slate::Context::ID(child->packagePath.string().c_str()),
                        cr, child->assetName, csel)) {
                    m_currentFolder = child->packagePath;
                }
                drawFolderIcon(cr.x + 4.0f, cr.y);
                y += kRowH + 2.0f;
            }
        }
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
    if (!m_registry) {
        // Registry not wired yet — show a hint and return.
        Slate::Theme& t = ui.theme();
        ui.Panel(body, t.windowBg);
        ui.LabelIn({body.x, body.y + body.h * 0.5f - 11.0f, body.w, 22.0f},
                   "Content Browser: no asset registry wired.", t.textDim,
                   Align::Center);
        return;
    }
    Rect toolbar{body.x, body.y, body.w, kToolbarH};
    Rect breadcrumb{body.x, toolbar.Bottom(), body.w, kBreadcrumbH};
    f32 bodyTop = breadcrumb.Bottom();
    Rect treePane{body.x, bodyTop, kTreePaneW, body.Bottom() - bodyTop};
    Rect gridPane{treePane.Right(), bodyTop, body.w - kTreePaneW,
                  body.Bottom() - bodyTop};

    DrawToolbar(ui, toolbar);
    DrawBreadcrumb(ui, breadcrumb);
    DrawTreePane(ui, treePane, ctx);
    DrawGridPane(ui, gridPane, ctx);
}

}  // namespace Luma::Editor::Panels
