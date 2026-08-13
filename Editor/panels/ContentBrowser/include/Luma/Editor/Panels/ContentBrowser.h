#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Luma/Asset/AssetData.h"
#include "Luma/Asset/AssetId.h"
#include "Luma/Asset/AssetRegistry.h"
#include "Luma/RHI/Renderer.h"
#include "PanelContext.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

// Content Browser panel — two-pane asset explorer wired to a Luma::AssetRegistry.
// Left pane: directory tree. Right pane: filtered grid + name list of the
// currently selected folder's assets. Search + type chips at top. The panel
// owns navigation state (current folder, name filter, type filter) and
// reports the user's currently-selected asset via a callback on PanelContext.
//
// Lifecycle: construct once per editor session. Each frame:
//   1. EditorScreen adds the project's content root to m_registry and
//      calls Scan() if it hasn't already (or on file-watcher refresh).
//   2. EditorScreen invokes panel->Draw(ui, body, ctx) with the docked
//      body rect. The panel draws breadcrumbs + toolbar + two panes.
//   3. EditorScreen reads panel->Selected() for the inspector / preview.

namespace Luma::Editor::Panels {

class ContentBrowserPanel {
public:
    ContentBrowserPanel();

    // Wire a registry pointer. The panel does not own the registry; the
    // caller (EditorScreen) decides when to Scan() / RefreshPath().
    void SetRegistry(AssetRegistry* registry) { m_registry = registry; }
    AssetRegistry* Registry() const noexcept { return m_registry; }

    // Loads the four PNGs the panel wants (sort up/down, search glass,
    // folder). Called once by EditorScreen after the renderer is ready;
    // missing files just leave the panel falling back to procedural
    // glyphs. The panel does not own the renderer or the textures.
    void SetIcons(Luma::TextureHandle sortUp, Luma::TextureHandle sortDown,
                  Luma::TextureHandle searchGlass,
                  Luma::TextureHandle folder);

    // Clears the navigation stack (e.g. after the project is reloaded).
    void ResetNavigation();

    void Draw(Slate::Context& ui, const Slate::Rect& body, PanelContext& ctx);

    // The asset the user last activated (double-clicked). EditorScreen
    // reads this to drive the inspector / preview.
    AssetId Selected() const noexcept { return m_selected; }

    // The folder the right pane is currently displaying (root if empty).
    std::filesystem::path CurrentFolder() const noexcept {
        return m_currentFolder;
    }

private:
    // Per-frame state mutated by Draw.
    AssetRegistry* m_registry = nullptr;
    std::filesystem::path m_currentFolder;  // empty = root of first root
    std::string m_nameFilter;
    std::optional<AssetType> m_typeFilter;  // nullopt = all
    AssetId m_selected{};
    bool m_showCreateMenu = false;

    // Per-type sort direction (true = ascending, false = descending). Drives
    // the small Up/Down arrow buttons in the toolbar's right side.
    bool m_sortAscending = true;

    // Loaded textures supplied by EditorScreen; 0 = not loaded.
    Luma::TextureHandle m_texSortUp = 0;
    Luma::TextureHandle m_texSortDown = 0;
    Luma::TextureHandle m_texSearchGlass = 0;
    Luma::TextureHandle m_texFolder = 0;

    // Layout constants.
    static constexpr f32 kToolbarH = 36.0f;
    static constexpr f32 kBreadcrumbH = 26.0f;
    static constexpr f32 kTreePaneW = 220.0f;
    static constexpr f32 kRowH = 24.0f;
    static constexpr f32 kTileSize = 80.0f;
    static constexpr f32 kTileGap = 8.0f;

    // Draw helpers (called from Draw).
    void DrawToolbar(Slate::Context& ui, const Slate::Rect& rect);
    void DrawBreadcrumb(Slate::Context& ui, const Slate::Rect& rect);
    void DrawTreePane(Slate::Context& ui, const Slate::Rect& rect,
                      PanelContext& ctx);
    void DrawGridPane(Slate::Context& ui, const Slate::Rect& rect,
                      PanelContext& ctx);
    void DrawTypeChips(Slate::Context& ui, const Slate::Rect& rect);

    // Returns the entries the right pane should currently show, applying
    // name + type + folder filters via the registry.
    std::vector<const AssetData*> CurrentEntries() const;

    // Nav helpers.
    void NavigateTo(const std::filesystem::path& folder);
    void NavigateUp();
    std::vector<std::filesystem::path> BreadcrumbSegments() const;

    // Helper: map AssetType -> Slate::Icon for the row / tile.
    static Slate::Icon IconForType(AssetType t) noexcept;
};

}  // namespace Luma::Editor::Panels
