#include "Luma/Editor/Panels/FileSystemTree.h"

#include "Luma/Slate/Icons.h"

namespace Luma::Editor::Panels {

using Slate::Align;
using Slate::Icon;
using Slate::Rect;
using Slate::Vec2;

FileSystemTreePanel::FileSystemTreePanel() = default;

void FileSystemTreePanel::Draw(Slate::Context& ui, const Slate::Rect& rect,
                               const char* title) {
    Slate::Theme& t = ui.theme();
    // Dark recessed panel: darker than the surrounding body surface so it
    // reads as an inset folder panel inside its host. Half-weight border
    // keeps the panel visible without dominating the chrome.
    const Slate::Color panelBg = Slate::Darken(t.surface0, 0.25f);
    ui.PanelRounded(rect, panelBg, t.radius.md);
    ui.PanelRoundedBordered(rect, panelBg, t.outline, t.radius.md,
                            0.5f);

    // Everything below is clipped to the panel so rows never bleed past
    // the rounded edges.
    ui.PushClip(rect);

    // Header.
    ui.Heading({rect.x + 12.0f, rect.y + 8.0f, rect.w - 24.0f, 22.0f}, title,
               t.text);

    if (!m_registry) {
        ui.PopClip();
        return;
    }

    f32 y = rect.y + 36.0f;
    // Orange tint applied to the white folder PNG.
    const Slate::Color kFolderOrange{255, 178, 92, 255};

    for (const auto& root : m_registry->Roots()) {
        // Display label for the root is "Content" (the conventional name
        // of the registered root). Filename is a fallback when the
        // registry was wired with a non-Content root.
        std::string name = root.filename().string();
        if (name.empty() || name == "Content") name = "Content";
        bool sel = m_currentFolder.empty() || m_currentFolder == root;
        DrawFolderRow(ui, rect, name.c_str(), root, y, 8.0f, sel);
        if (y > rect.Bottom()) break;
        // Show direct child folders of this root when it's expanded
        // (i.e. selected here).
        if (sel) {
            auto children = m_registry->FilterByDirectory(root);
            for (const auto* child : children) {
                if (!child->IsFolder()) continue;
                bool csel = (m_currentFolder == child->packagePath);
                DrawFolderRow(ui, rect, child->assetName.c_str(),
                              child->packagePath, y, 24.0f, csel);
                if (y > rect.Bottom()) break;
            }
        }
    }
    ui.PopClip();
}

void FileSystemTreePanel::DrawFolderRow(Slate::Context& ui,
                                        const Slate::Rect& panel,
                                        const char* label,
                                        const std::filesystem::path& path,
                                        f32& y, f32 indent, bool selected) {
    constexpr f32 kIconSize = 16.0f;
    const Slate::Color kFolderOrange{255, 178, 92, 255};

    Rect r{panel.x + indent, y, panel.w - indent - 8.0f, kRowH};
    // Selectable handles hover fill + click detection; we set
    // currentFolder + fire the callback when it returns true.
    if (ui.Selectable(Slate::Context::ID(path.string().c_str()), r, label,
                      selected)) {
        m_currentFolder = path;
        if (OnFolderSelected) OnFolderSelected(path);
    }
    // Folder icon: supplied orange-tinted PNG when available, otherwise
    // the procedural Icon::Folder glyph.
    Rect ir{r.x + 4.0f, r.y + (kRowH - kIconSize) * 0.5f, kIconSize, kIconSize};
    if (m_texFolder) {
        ui.Image(m_texFolder, ir, kFolderOrange);
    } else {
        Slate::DrawIcon(ui, ir, Icon::Folder, kFolderOrange);
    }
    y += kRowH + 2.0f;
}

}  // namespace Luma::Editor::Panels
