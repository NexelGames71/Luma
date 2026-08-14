#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include "Luma/Asset/AssetRegistry.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Types.h"

// FileSystemTreePanel — a reusable "Folders" tree column backed by a
// Luma::AssetRegistry. Reads the registered roots + their child
// folders, lets the user click a folder, and reports the selection
// via a callback + a getter. Originally split out of the Content
// Browser so the same folder-tree UI can be dropped into any other
// panel (an Import dialog, a Save-as picker, a standalone asset
// explorer, etc.).
//
// The panel paints its own flat-black column background with no
// border or rounded corners, matching Unreal's Content Browser
// Folders column. Row hover/select feedback comes from the regular
// Selectable widget. The host decides where the column starts and
// stops (no implicit margin).
//
// Lifecycle: construct once, wire a registry + (optionally) an orange
// folder PNG texture via SetFolderTexture, then each frame call
// Draw(ui, rect). Read SelectedFolder() (or hook OnFolderSelected) to
// react to clicks. The panel does NOT own the registry or textures.

namespace Luma::Editor::Panels {

class FileSystemTreePanel {
public:
    FileSystemTreePanel();

    // Wire the asset registry the tree reads roots + child folders from.
    void SetRegistry(AssetRegistry* registry) { m_registry = registry; }
    AssetRegistry* Registry() const noexcept { return m_registry; }

    // Optional orange folder PNG (legacy). 0 = fall back to the
    // procedural Icon::Folder glyph (so the panel works without a
    // renderer-loaded texture).
    void SetFolderTexture(Luma::TextureHandle folder) {
        m_texFolder = folder;
    }
    Luma::TextureHandle FolderTexture() const noexcept {
        return m_texFolder;
    }

    // Optional "open folder" PNG used as the row icon. When set, it
    // overrides the procedural Icon::Folder and the legacy orange-tinted
    // m_texFolder; drawn without a color tint so the PNG's built-in
    // colors come through.
    void SetOpenFolderTexture(Luma::TextureHandle openFolder) {
        m_texOpenFolder = openFolder;
    }
    Luma::TextureHandle OpenFolderTexture() const noexcept {
        return m_texOpenFolder;
    }

    // Fired once when the user clicks a folder. Use SelectedFolder() to
    // read the new value inside the callback.
    std::function<void(const std::filesystem::path&)> OnFolderSelected;

    // Read / write the current selection. Setting it programmatically
    // does NOT fire OnFolderSelected (the user didn't click).
    const std::filesystem::path& SelectedFolder() const noexcept {
        return m_currentFolder;
    }
    void SetSelectedFolder(const std::filesystem::path& folder) {
        m_currentFolder = folder;
    }

    // Render the tree inside `rect` (already inset by the host if it
    // wants a margin). Fills `rect` with a flat black background (no
    // border, no rounded corners — matches Unreal's Content Browser
    // Folders column) and then draws the title + the folder rows.
    // `title` overrides the header label (default "Folders").
    void Draw(Slate::Context& ui, const Slate::Rect& rect,
              const char* title = "Folders");

private:
    AssetRegistry* m_registry = nullptr;
    std::filesystem::path m_currentFolder;  // empty = first root
    Luma::TextureHandle m_texFolder = 0;
    Luma::TextureHandle m_texOpenFolder = 0;

    // Layout constants.
    static constexpr f32 kRowH = 24.0f;

    // Draws one folder row + its icon; updates `y` and currentFolder on
    // click. `indent` is the left indent in px.
    void DrawFolderRow(Slate::Context& ui, const Slate::Rect& panel,
                       const char* label,
                       const std::filesystem::path& path, f32& y, f32 indent,
                       bool selected);
};

}  // namespace Luma::Editor::Panels
