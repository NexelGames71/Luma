#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Luma/Asset/AssetData.h"
#include "Luma/Asset/AssetId.h"
#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Editor/Panels/FileSystemTree.h"
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
    
    // Wire renderer for thumbnail generation
    void SetRenderer(Luma::Renderer* renderer);

    // Loads the chrome PNGs the panel wants (sort up/down, search glass,
    // folder, reload, import, open-folder). Called once by EditorScreen
    // after the renderer is ready; missing files just leave the panel
    // falling back to procedural glyphs. The panel does not own the
    // renderer or the textures.
    void SetIcons(Luma::TextureHandle sortUp, Luma::TextureHandle sortDown,
                  Luma::TextureHandle searchGlass,
                  Luma::TextureHandle folder,
                  Luma::TextureHandle reload,
                  Luma::TextureHandle importTex,
                  Luma::TextureHandle openFolder,
                  Luma::TextureHandle expandArrow,
                  Luma::TextureHandle retractArrow);

    // Clears the navigation stack (e.g. after the project is reloaded).
    void ResetNavigation();

    void Draw(Slate::Context& ui, const Slate::Rect& body, PanelContext& ctx);

    // Right-click create menu overlay — drawn by EditorScreen AFTER the
    // dock (like the outliner's create menu) so it isn't clipped to this
    // panel's rect. No-op when the menu isn't open.
    void DrawFloatingMenu(Slate::Context& ui, PanelContext& ctx);

    // Selects the given asset (single-click selection, not activation).
    void SetSelected(const AssetId& id) { m_selected = id; }

    // The asset the user last activated (double-clicked). EditorScreen
    // reads this to drive the inspector / preview.
    AssetId Selected() const noexcept { return m_selected; }

    // The asset the user double-clicked this frame (single-click selection
    // stays in Selected()). EditorScreen polls this to open the right
    // editor (e.g. Material Editor for .lmat). Cleared by ClearActivated().
    AssetId Activated() const noexcept { return m_activated; }
    void ClearActivated() { m_activated = AssetId{}; }

    // The folder the right pane is currently displaying (root if empty).
    std::filesystem::path CurrentFolder() const noexcept {
        return m_currentFolder;
    }
    
    // Thumbnail helpers
    u64 GetThumbnailTexture(const AssetId& assetId, const std::filesystem::path& nativePath, Luma::Renderer* renderer);
    void RequestThumbnail(const AssetId& assetId, const std::filesystem::path& nativePath);

private:
    // Per-frame state mutated by Draw.
    AssetRegistry* m_registry = nullptr;
    std::filesystem::path m_currentFolder;  // empty = root of first root
    std::string m_nameFilter;
    std::optional<AssetType> m_typeFilter;  // nullopt = all
    AssetId m_selected{};
    AssetId m_activated{};  // double-clicked this frame

    // Reusable folder tree (draws the dark inset "Folders" panel and
    // reports the selected folder via OnFolderSelected). Kept in sync
    // with m_currentFolder both ways.
    FileSystemTreePanel m_tree;
    // Fraction (0..1) of the body area's width occupied by the tree
    // pane. Mutated by the SplitterV so the user can resize the tree
    // vs. grid columns. Clamped by SplitterV to [0.12, 0.88].
    float m_treeRatio = 0.22f;

    // Filter drop-down state: true while the popup listing asset-type
    // filters is open. Toggled by the down/up chevron button inside the
    // search bar; dismissed by selection or outside-click.
    bool m_filterMenuOpen = false;
    // Vertical scroll offset of the asset grid (px). Clamped to the grid's
    // content height by VerticalScroll each frame.
    f32 m_gridScroll = 0.0f;
    // True between a press over a breadcrumb segment and its release, so a
    // press-then-drag-off release doesn't navigate.
    bool m_breadPressed = false;
    // True between a press over the reload icon and its release, so a
    // press-then-drag-off release doesn't trigger a scan.
    bool m_reloadPressed = false;
    // True between a press over the Import button and its release, so a
    // press-then-drag-off release doesn't fire Import.
    bool m_importPressed = false;
    // Rect of the toggle button when the menu was last opened; the popup
    // anchors below it. Refreshed by DrawToolbar each frame.
    Slate::Rect m_filterAnchor{};

    // Right-click "Create" context menu state. The menu floats (drawn by
    // DrawFloatingMenu after the dock) so it isn't clipped to the panel.
    bool m_contextMenuOpen = false;
    bool m_contextMenuOpenedThisFrame = false;
    Slate::Vec2 m_contextMenuPos{};  // anchor (screen coords)
    std::string m_contextSearch;
    int m_contextHover = -1;      // hovered row (flat index, -1 = none)
    int m_contextFocus = -1;      // keyboard focus (flat index, -1 = none)
    int m_contextSubmenu = -1;    // open category index (-1 = none)
    u64 m_pressedCreateRow = 0;  // press-tracking id for menu rows
    // Body rect cached by Draw so the floating pass can clamp the menu.
    Slate::Rect m_bodyRect{};

    // Loaded textures supplied by EditorScreen; 0 = not loaded. The sort
    // up/down PNGs are kept for API stability but the toolbar chevron is
    // drawn with procedural icons (crisp at any DPI).
    Luma::TextureHandle m_texSortUp = 0;
    Luma::TextureHandle m_texSortDown = 0;
    Luma::TextureHandle m_texSearchGlass = 0;
    Luma::TextureHandle m_texFolder = 0;
    Luma::TextureHandle m_texReload = 0;
    Luma::TextureHandle m_texImport = 0;
    Luma::TextureHandle m_texOpenFolder = 0;
    Luma::TextureHandle m_texExpandArrow = 0;
    Luma::TextureHandle m_texRetractArrow = 0;

    // Renderer for thumbnail generation
    Luma::Renderer* m_renderer = nullptr;
    
    // Thumbnail cache (asset ID -> texture handle)
    std::unordered_map<AssetId, u64> m_thumbnailCache;

    // Layout constants.
    static constexpr f32 kToolbarH = 36.0f;
    // Card dimensions for the asset grid (Unreal-style): a thumbnail on
    // top + name + asset type rows below.
    static constexpr f32 kTileW = 84.0f;
    static constexpr f32 kTileH = 138.0f;
    static constexpr f32 kTileGap = 6.0f;

    // Draw helpers (called from Draw).
    void DrawToolbar(Slate::Context& ui, const Slate::Rect& rect);
    void DrawGridPane(Slate::Context& ui, const Slate::Rect& rect,
                      PanelContext& ctx);
    // Draws the type-filter drop-down below `anchor`. No-op when
    // m_filterMenuOpen is false; handles outside-click to close and
    // updates m_typeFilter on selection.
    void DrawFilterMenu(Slate::Context& ui, const Slate::Rect& anchor);
    // Draws the right-click create menu (Unreal-style: search + GET section
    // with Import, CREATE section with New Folder / Material, then a
    // categorized list whose rows open a submenu beside them). Called from
    // DrawFloatingMenu.
    void DrawCreateMenu(Slate::Context& ui, PanelContext& ctx,
                        bool openedThisFrame);

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
