#include "Luma/Editor/Panels/FileSystemTree.h"
#include <filesystem>
#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Asset/AssetData.h"
#include "Luma/Core/Log.h"
#include "Luma/Slate/Theme.h"

using namespace Luma::Slate;

namespace Luma::Editor::Panels {

void FileSystemTreePanel::SetFolderTexture(TextureHandle tex) {
    m_folderTex = tex;
}

void FileSystemTreePanel::SetOpenFolderTexture(TextureHandle tex) {
    m_openFolderTex = tex;
}

void FileSystemTreePanel::SetExpandTexture(TextureHandle tex) {
    m_expandTex = tex;
}

void FileSystemTreePanel::SetRetractTexture(TextureHandle tex) {
    m_retractTex = tex;
}

void FileSystemTreePanel::SetSelectedFolder(const std::filesystem::path& folder) {
    m_currentFolder = folder;
}

void FileSystemTreePanel::SetRegistry(Luma::AssetRegistry* registry) {
    m_registry = registry;
}

void FileSystemTreePanel::Draw(Slate::Context& ui, const Slate::Rect& body) {
    if (!m_registry) return;
    
    constexpr f32 kHeaderHeight = 28.0f;
    
    Slate::Theme& t = ui.theme();
    
    // Draw header with "My Project" title (UE-style dark header)
    Slate::Rect headerRect{body.x, body.y, body.w, kHeaderHeight};
    ui.Panel(headerRect, t.surface2);
    ui.Panel({headerRect.x, headerRect.Bottom() - 1.0f, headerRect.w, 1.0f}, t.separator);
    
    // Draw "My Project" title in header
    constexpr f32 kTitlePadX = 8.0f;
    Slate::Vec2 titlePos{headerRect.x + kTitlePadX, headerRect.y + (kHeaderHeight / 2.0f) - 7.0f};
    ui.Label(titlePos, "My Project", t.text);
    
    // Calculate tree content area (below header)
    Slate::Rect treeRect{body.x, body.y + kHeaderHeight, body.w, body.h - kHeaderHeight};

    // Draw darker background for tree content area
    ui.Panel(treeRect, Slate::Color::RGB(15, 15, 15));

    ui.PushClip(treeRect);
    
    // Rows shift up by the scroll offset; the clip keeps them inside the
    // tree area (below the header). All rows are drawn so the content
    // height is measured correctly for the scrollbar.
    f32 y = treeRect.y - m_scroll;
    
    // Keep the current selection's ancestors expanded
    EnsureRevealed(m_currentFolder);
    
    for (const auto& root : m_registry->Roots()) {
        // The registry root is the project's Assets/ folder at the project
        // root (not a Content/ subdirectory).
        std::string name = root.filename().string();
        if (name.empty() || name == "Assets") name = "Assets";
        
        DrawFolderBranch(ui, treeRect, name.c_str(), root, y, 0);
    }
    
    ui.PopClip();

    // Scroll region spans the folder list area below the header. Content
    // height is recovered from where the row cursor ended.
    f32 contentH = (y - treeRect.y) + m_scroll;
    m_scroll = ui.VerticalScroll(Slate::Context::ID("fs.tree"), treeRect,
                                 contentH, m_scroll);
}

void FileSystemTreePanel::DrawFolderBranch(Slate::Context& ui,
                                           const Slate::Rect& panel,
                                           const char* label,
                                           const std::filesystem::path& path,
                                           f32& y, int depth) {
    bool selected = (m_currentFolder == path);
    bool hasChildren = HasChildFolders(path);
    bool expanded = hasChildren && IsExpanded(path);
    
    DrawFolderRow(ui, panel, label, path, y, depth, selected, hasChildren, expanded);
    
    if (expanded && m_registry) {
        // Get all entries under this path and filter for folders
        auto allEntries = m_registry->Filter(std::nullopt, path, "");
        for (const auto* child : allEntries) {
            if (!child->IsFolder()) continue;
            DrawFolderBranch(ui, panel, child->assetName.c_str(),
                             child->packagePath, y, depth + 1);
        }
    }
}

void FileSystemTreePanel::DrawFolderRow(Slate::Context& ui,
                                        const Slate::Rect& panel,
                                        const char* label,
                                        const std::filesystem::path& path,
                                        f32& y, int depth, bool selected,
                                        bool hasChildren, bool expanded) {
    (void)selected;  // Used by Selectable
    constexpr f32 kIconSize = 16.0f;
    constexpr f32 kArrowW = 12.0f;
    const Slate::Color kFolderOrange{255, 178, 92, 255};
    const Slate::Theme& t = ui.theme();
    
    f32 indent = 4.0f + depth * 14.0f;
    f32 arrowX = panel.x + indent;
    f32 iconX = panel.x + indent + kArrowW;
    f32 labelX = iconX + kIconSize + t.space.sm;
    
    Rect iconR{iconX, y + (kRowH - kIconSize) * 0.5f, kIconSize, kIconSize};
    Rect labelR{labelX, y, panel.w - labelX, kRowH};
    
    // Expand/retract toggle - handle before selectable to avoid conflict
    if (hasChildren) {
        Rect arrowHit{panel.x + indent, y, kArrowW, kRowH};
        bool arrowHover = arrowHit.Contains(ui.mouse());
        if (arrowHover) ui.RequestCursor(Luma::CursorShape::Hand);

        // Manual click detection for arrow (no button background)
        static std::string arrowPressedPath;
        if (ui.mousePressed(0) && arrowHover) {
            arrowPressedPath = path.string();
        }
        if (ui.mouseReleased(0) && arrowPressedPath == path.string() && arrowHover) {
            std::string key = path.lexically_normal().string();
            if (expanded)
                m_expanded.erase(key);
            else
                m_expanded.insert(key);
            arrowPressedPath.clear();
        }

        if (expanded && m_retractTex) {
            Rect ar{arrowX, y + (kRowH - 5.0f) * 0.5f, 10.0f, 5.0f};
            ui.Image(m_retractTex, ar, t.textDim);
        } else if (!expanded && m_expandTex) {
            Rect ar{arrowX, y + (kRowH - 8.0f) * 0.5f, 8.0f, 8.0f};
            ui.Image(m_expandTex, ar, t.textDim);
        }
    } else {
        // No children - still show the folder row but without arrow
    }
    
    // Selectable handles hover + click for the label area
    if (ui.Selectable(Slate::Context::ID(path.string().c_str()), labelR, label, selected)) {
        m_currentFolder = path;
        if (OnFolderSelected) OnFolderSelected(path);
    }
    
    // Folder icon
    if (m_openFolderTex) {
        ui.Image(m_openFolderTex, iconR);
    } else if (m_folderTex) {
        ui.Image(m_folderTex, iconR, kFolderOrange);
    }
    
    y += kRowH + 1.0f;
}

void FileSystemTreePanel::EnsureRevealed(const std::filesystem::path& folder) {
    if (folder.empty() || !m_registry) return;
    auto f = folder.lexically_normal();
    
    for (const auto& root : m_registry->Roots()) {
        auto r = root.lexically_normal();
        auto rel = f.lexically_relative(r);
        auto relStr = rel.string();
        
        if (relStr.empty() || relStr.front() == '.' || relStr.find("..") != std::string::npos) {
            continue;
        }
        
        std::filesystem::path acc = r;
        m_expanded.insert(acc.string());
        
        for (auto part : rel) {
            acc /= part;
            m_expanded.insert(acc.string());
        }
        return;
    }
}

bool FileSystemTreePanel::HasChildFolders(const std::filesystem::path& path) const {
    if (!m_registry) return false;
    auto allEntries = m_registry->Filter(std::nullopt, path, "");
    for (const auto* entry : allEntries) {
        if (entry->IsFolder()) return true;
    }
    return false;
}

bool FileSystemTreePanel::IsExpanded(const std::filesystem::path& path) const {
    return m_expanded.find(path.lexically_normal().string()) != m_expanded.end();
}

std::filesystem::path FileSystemTreePanel::SelectedFolder() const {
    return m_currentFolder;
}

}  // namespace Luma::Editor::Panels
