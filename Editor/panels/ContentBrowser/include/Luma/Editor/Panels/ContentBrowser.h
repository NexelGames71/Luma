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
// Left pane: directory tree. Right pane: filtered grid of the currently
// selected folder's assets. Toolbar holds a search field whose right edge
// carries a down/up chevron that toggles a type-filter drop-down (Mesh / 3D
// model, Material, Texture, Scene, Shaders, Audios, plus All). The panel
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

    // Filter drop-down state: true while the popup listing asset-type
    // filters is open. Toggled by the down/up chevron button inside the
    // search bar; dismissed by selection or outside-click.
    bool m_filterMenuOpen = false;

    // Loaded textures supplied by EditorScreen; 0 = not loaded. The sort
    // up/down PNGs are kept for API stability but the toolbar chevron is
    // drawn with procedural icons (crisp at any DPI).
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
    // Draws the type-filter drop-down below `anchor`. No-op when
    // m_filterMenuOpen is false; handles outside-click to close and
    // updates m_typeFilter on selection.
    void DrawFilterMenu(Slate::Context& ui, const Slate::Rect& anchor);

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
