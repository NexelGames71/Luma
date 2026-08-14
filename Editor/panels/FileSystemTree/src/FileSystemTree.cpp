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
    // Folders column shares the same body bg as the surrounding content
    // browser surface so its header reads as the same color as the other
    // editor panels (no recessed dark fill).
    ui.PanelRounded(rect, t.surface1, t.radius.md);

    // Clip rows to the panel rect so they never bleed past the column.
    ui.PushClip(rect);

    // Header.
    ui.Heading({rect.x + 6.0f, rect.y + 4.0f, rect.w - 12.0f, 22.0f}, title,
               t.text);

    // Separator + darker strip under it so the header reads as its own
    // band and the folder list below sits on a darker recessed surface.
    constexpr f32 kHeaderH = 28.0f;
    ui.Panel({rect.x + 4.0f, rect.y + kHeaderH - 1.0f, rect.w - 8.0f, 1.0f},
             t.separator);
    Rect listR{rect.x, rect.y + kHeaderH, rect.w,
               rect.Bottom() - (rect.y + kHeaderH)};
    ui.Panel(listR, Slate::Darken(t.surface1, 0.3f));

    if (!m_registry) {
        ui.PopClip();
        return;
    }

    f32 y = rect.y + 28.0f;
    // Orange tint applied to the white folder PNG.
    const Slate::Color kFolderOrange{255, 178, 92, 255};

    for (const auto& root : m_registry->Roots()) {
        // Display label for the root is "Content" (the conventional name
        // of the registered root). Filename is a fallback when the
        // registry was wired with a non-Content root.
        std::string name = root.filename().string();
        if (name.empty() || name == "Content") name = "Content";
        bool sel = m_currentFolder.empty() || m_currentFolder == root;
        DrawFolderRow(ui, rect, name.c_str(), root, y, 4.0f, sel);
        if (y > rect.Bottom()) break;
        // Show direct child folders of this root when it's expanded
        // (i.e. selected here).
        if (sel) {
            auto children = m_registry->FilterByDirectory(root);
            for (const auto* child : children) {
                if (!child->IsFolder()) continue;
                bool csel = (m_currentFolder == child->packagePath);
                DrawFolderRow(ui, rect, child->assetName.c_str(),
                              child->packagePath, y, 20.0f, csel);
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

    Rect r{panel.x + indent, y, panel.w - indent - 4.0f, kRowH};
    // Selectable handles hover fill + click detection; we set
    // currentFolder + fire the callback when it returns true.
    if (ui.Selectable(Slate::Context::ID(path.string().c_str()), r, label,
                      selected)) {
        m_currentFolder = path;
        if (OnFolderSelected) OnFolderSelected(path);
    }
    // Folder icon: supplied orange-tinted PNG when available, otherwise
    // the procedural Icon::Folder glyph.
    Rect ir{r.x + 2.0f, r.y + (kRowH - kIconSize) * 0.5f, kIconSize, kIconSize};
    if (m_texFolder) {
        ui.Image(m_texFolder, ir, kFolderOrange);
    } else {
        Slate::DrawIcon(ui, ir, Icon::Folder, kFolderOrange);
    }
    y += kRowH + 1.0f;
}

}  // namespace Luma::Editor::Panels
