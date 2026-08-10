#pragma once

#include <filesystem>
#include <optional>

#include "Luma/Project/Project.h"
#include "Luma/Slate/Context.h"

namespace Luma {

// The editor shell: menu bar, world outliner, viewport, inspector, and a
// console/asset browser panel. Milestone-6 features fill these in; for now it
// is the docked placeholder layout that opens with a project loaded.
class EditorScreen {
public:
    explicit EditorScreen(const std::filesystem::path& projectFile);

    bool HasProject() const { return m_project.has_value(); }
    void Draw(Slate::Context& ui, f32 width, f32 height);

private:
    std::optional<Project> m_project;
    std::string m_title;
};

}  // namespace Luma
