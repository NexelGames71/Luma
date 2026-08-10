#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Luma/Project/Project.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Image.h"

namespace Luma {

// Result of a browser frame: when `launch` is set, the app should open the
// editor for `projectFile` (Godot-style: relaunch the exe with --project).
struct BrowserResult {
    bool launch = false;
    std::filesystem::path projectFile;
};

// The project browser screen (Cave-style): tabs for Your Projects / New Project
// / About, project creation with template selection, and opening existing
// projects. Drawn entirely with Luma Slate.
class ProjectBrowser {
public:
    ProjectBrowser();

    // Optional banner logo (loaded by the app).
    void SetLogo(const Slate::Image& logo) { m_logo = logo; }

    BrowserResult Draw(Slate::Context& ui, f32 width, f32 height);

private:
    void DrawNewProject(Slate::Context& ui, const Slate::Rect& content,
                        BrowserResult& result);
    void DrawYourProjects(Slate::Context& ui, const Slate::Rect& content,
                          BrowserResult& result);
    void DrawAbout(Slate::Context& ui, const Slate::Rect& content);
    void Rescan();

    struct ProjectEntry {
        std::string name;
        std::string templateName;
        std::filesystem::path file;
    };

    void CreateFromInputs(BrowserResult& result);

    int m_tab = 1;  // 0 = Your Projects, 1 = New Project, 2 = About
    std::string m_name = "Ancient Simulation";
    std::string m_directory;
    GameTemplate m_template = GameTemplate::ThirdPerson;
    std::string m_status;

    // Setup Options (collapsible footer section).
    bool m_setupOpen = false;
    bool m_vsync = true;
    bool m_gitInit = false;
    bool m_starterContent = true;

    std::vector<ProjectEntry> m_projects;
    bool m_scanned = false;

    Slate::Image m_logo;
};

}  // namespace Luma
