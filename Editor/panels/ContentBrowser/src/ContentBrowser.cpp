#include "Luma/Editor/Panels/ContentBrowser.h"

#include <algorithm>
#include <cctype>
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

    // Type chips: a row of toggles for filtering by AssetType.
    f32 chipX = rect.x + 80.0f;
    f32 chipY = rect.y + 6.0f;
    f32 chipH = 24.0f;
    auto drawChip = [&](u64 id, const char* label, AssetType type,
                        f32 width) {
        Rect chip{chipX, chipY, width, chipH};
        bool active =
            (!m_typeFilter.has_value() && type == AssetType::Unknown) ||
            (m_typeFilter.has_value() && m_typeFilter.value() == type);
        if (ui.Tab(id, chip, label, active)) {
            if (type == AssetType::Unknown) m_typeFilter.reset();
            else m_typeFilter = type;
        }
        chipX += width + 4.0f;
    };
    const Slate::Font& f = ui.uiFont();
    drawChip(Slate::Context::ID("cb.t.all"), "All", AssetType::Unknown,
             f.Measure("All").x + 18.0f);
    drawChip(Slate::Context::ID("cb.t.tex"), "Tex", AssetType::Texture,
             f.Measure("Tex").x + 18.0f);
    drawChip(Slate::Context::ID("cb.t.mesh"), "Mesh", AssetType::Mesh,
             f.Measure("Mesh").x + 18.0f);
    drawChip(Slate::Context::ID("cb.t.mat"), "Mat", AssetType::Material,
             f.Measure("Mat").x + 18.0f);
    drawChip(Slate::Context::ID("cb.t.sh"), "Shd", AssetType::Shader,
             f.Measure("Shd").x + 18.0f);
    drawChip(Slate::Context::ID("cb.t.scn"), "Scn", AssetType::Scene,
             f.Measure("Scn").x + 18.0f);
    drawChip(Slate::Context::ID("cb.t.snd"), "Snd", AssetType::Sound,
             f.Measure("Snd").x + 18.0f);
    (void)f;

    // Sort arrows: visible on the right of the search box when a sortable
    // type is active. Clicking either toggles sort direction. Only the
    // types listed in the brief are sortable (Texture/Mesh/Material/
    // Shader/Scene/Sound); other types fall back to ascending.
    bool sortable = m_typeFilter.has_value() &&
                    (m_typeFilter.value() == AssetType::Texture ||
                     m_typeFilter.value() == AssetType::Mesh ||
                     m_typeFilter.value() == AssetType::Material ||
                     m_typeFilter.value() == AssetType::Shader ||
                     m_typeFilter.value() == AssetType::Scene ||
                     m_typeFilter.value() == AssetType::Sound);
    constexpr f32 kArrowSize = 22.0f;
    f32 arrowGap = 4.0f;
    f32 rightEdge = rect.Right() - 8.0f;
    Rect upArrowR{}, downArrowR{};
    bool arrowHoverUp = false, arrowHoverDown = false;
    if (sortable) {
        upArrowR = Rect{rightEdge - kArrowSize, rect.y + 7.0f, kArrowSize,
                        kArrowSize};
        downArrowR = Rect{upArrowR.x - kArrowSize - arrowGap, rect.y + 7.0f,
                          kArrowSize, kArrowSize};
        // Background pills for the arrows so they read as buttons.
        ui.PanelRoundedBordered(upArrowR, m_sortAscending ? t.accentMuted
                                                          : t.surface3,
                                t.outline, t.radius.sm, t.border.hairline);
        ui.PanelRoundedBordered(downArrowR, m_sortAscending ? t.surface3
                                                            : t.accentMuted,
                                t.outline, t.radius.sm, t.border.hairline);
        // The two buttons share a hit zone with each pill (so we treat them
        // as two distinct selectable rows the panel can detect).
        if (ui.Selectable(Slate::Context::ID("cb.sort.up"), upArrowR, "",
                          m_sortAscending, Icon::ChevronUp)) {
            m_sortAscending = true;
        }
        if (ui.Selectable(Slate::Context::ID("cb.sort.down"), downArrowR, "",
                          !m_sortAscending, Icon::ChevronDown)) {
            m_sortAscending = false;
        }
        rightEdge = downArrowR.x - arrowGap;
    }

    // Search box on the right of the toolbar, ending before the arrows.
    f32 searchW = std::min(260.0f, rightEdge - (rect.x + 76.0f));
    if (searchW < 100.0f) searchW = 100.0f;
    Rect searchR{rightEdge - searchW, rect.y + 6.0f, searchW, 24.0f};
    ui.SearchBox(Slate::Context::ID("cb.search"), searchR, m_nameFilter,
                 m_texSearchGlass, "Search assets...");
    (void)arrowHoverUp;
    (void)arrowHoverDown;
}

void ContentBrowserPanel::DrawBreadcrumb(Slate::Context& ui,
                                         const Slate::Rect& rect) {
    Slate::Theme& t = ui.theme();
    ui.Panel(rect, t.surface1);
    ui.Panel({rect.x, rect.Bottom() - 1.0f, rect.w, 1.0f}, t.separator);

    f32 x = rect.x + 8.0f;
    auto labelWidth = [&](std::string_view s) {
        return ui.uiFont().Measure(s).x + 14.0f;
    };

    // Root segment ("Content") - clicking returns to root.
    {
        Rect r{x, rect.y + 3.0f, labelWidth("Content"), 20.0f};
        if (ui.Button(Slate::Context::ID("cb.root"), r, "Content")) {
            m_currentFolder.clear();
        }
        x += r.w + 2.0f;
        ui.LabelIn({x, rect.y + 3.0f, 12.0f, 20.0f}, ">", t.textDim);
        x += 12.0f;
    }

    // Each segment uses its display path (relative to the Content root) for
    // the navigation id but only the leaf-name as the visible label, so the
    // breadcrumb never leaks the system root into the UI.
    if (!m_currentFolder.empty() && m_registry) {
        std::string rel = m_registry->DisplayPathFor(m_currentFolder);
        // Strip the leading "Content/" we already rendered.
        const std::string prefix = "Content/";
        if (rel.size() > prefix.size() && rel.substr(0, prefix.size()) == prefix)
            rel = rel.substr(prefix.size());
        std::stringstream ss(rel);
        std::string part;
        std::filesystem::path acc;
        while (std::getline(ss, part, '/')) {
            if (part.empty()) continue;
            acc /= part;
            Rect r{x, rect.y + 3.0f, labelWidth(part), 20.0f};
            if (ui.Button(Slate::Context::ID(acc.string().c_str()), r, part)) {
                // Reconstruct the absolute path from the Content root.
                if (!m_registry->Roots().empty()) {
                    NavigateTo(m_registry->Roots().front() / acc);
                }
            }
            x += r.w + 2.0f;
            ui.LabelIn({x, rect.y + 3.0f, 12.0f, 20.0f}, ">", t.textDim);
            x += 12.0f;
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

    // Sort: folders first, then by name (case-insensitive). Honors
    // m_sortAscending when the active type is sortable; otherwise always
    // ascending.
    std::sort(entries.begin(), entries.end(),
              [this](const AssetData* a, const AssetData* b) {
                  if (a->IsFolder() != b->IsFolder())
                      return a->IsFolder();
                  std::string an = a->assetName;
                  std::transform(an.begin(), an.end(), an.begin(),
                                 [](unsigned char c) { return std::tolower(c); });
                  std::string bn = b->assetName;
                  std::transform(bn.begin(), bn.end(), bn.begin(),
                                 [](unsigned char c) { return std::tolower(c); });
                  if (an == bn) return false;
                  return m_sortAscending ? an < bn : an > bn;
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
