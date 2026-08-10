#include "Luma/Editor/ProjectBrowser.h"

#include <array>
#include <cstdlib>

#include "Luma/Core/Log.h"

namespace Luma {
namespace fs = std::filesystem;
using Slate::Align;
using Slate::Color;
using Slate::Rect;

namespace {

fs::path DefaultProjectsRoot() {
    const char* profile = std::getenv("USERPROFILE");
    fs::path base = profile ? fs::path(profile) : fs::current_path();
    return base / "LumaProjects";
}

struct TemplateInfo {
    GameTemplate value;
    const char* title;
    const char* desc;
};

const std::array<TemplateInfo, 4>& Templates() {
    static const std::array<TemplateInfo, 4> kTemplates = {{
        {GameTemplate::Empty, "Empty", "Bare minimum setup."},
        {GameTemplate::FirstPerson, "First Person", "FPS controller + camera."},
        {GameTemplate::ThirdPerson, "Third Person", "Character, IK + camera."},
        {GameTemplate::TopDown, "Top Down", "Top-down character + camera."},
    }};
    return kTemplates;
}

}  // namespace

ProjectBrowser::ProjectBrowser() {
    m_directory = DefaultProjectsRoot().string();
}

void ProjectBrowser::Rescan() {
    m_projects.clear();
    for (const fs::path& file : DiscoverProjects(DefaultProjectsRoot())) {
        std::string err;
        if (auto project = Project::Load(file, &err)) {
            m_projects.push_back({project->Name(),
                                  ToString(project->Template()), file});
        }
    }
    m_scanned = true;
}

BrowserResult ProjectBrowser::Draw(Slate::Context& ui, f32 width, f32 height) {
    if (!m_scanned) Rescan();
    BrowserResult result;
    Slate::Theme& t = ui.theme();

    // Background.
    ui.Panel({0, 0, width, height}, t.windowBg);

    // Header bar with wordmark.
    Rect header{0, 0, width, 64};
    ui.Panel(header, t.header);
    ui.LabelIn({24, 0, 300, 64}, "LUMA", t.accentText, Align::Left, true);
    ui.LabelIn({0, 0, width - 24, 64}, "Engine", t.textDim, Align::Right);

    // Tabs.
    const char* tabNames[] = {"Your Projects", "New Project", "About"};
    f32 tabW = 150.0f;
    for (int i = 0; i < 3; ++i) {
        Rect r{24 + i * tabW, 64, tabW, 40};
        if (ui.Tab(Slate::Context::ID(tabNames[i]), r, tabNames[i],
                   m_tab == i)) {
            m_tab = i;
        }
    }
    Rect content{24, 120, width - 48, height - 120 - 72};
    ui.PanelBordered(content, t.panelBg, t.panelBorder, 1.0f);

    if (m_tab == 0) DrawYourProjects(ui, content, result);
    else if (m_tab == 1) DrawNewProject(ui, content, result);
    else DrawAbout(ui, content);

    // Status / footer bar.
    Rect footer{0, height - 56, width, 56};
    ui.Panel(footer, t.header);
    if (!m_status.empty()) {
        ui.LabelIn({24, height - 56, width - 300, 56}, m_status,
                   Color::RGB(220, 120, 120), Align::Left);
    }
    return result;
}

void ProjectBrowser::DrawNewProject(Slate::Context& ui, const Rect& content,
                                    BrowserResult& result) {
    Slate::Theme& t = ui.theme();
    f32 x = content.x + 24.0f;
    f32 y = content.y + 24.0f;
    f32 fieldW = content.w - 48.0f;

    ui.LabelIn({x, y, 120, 28}, "Name", t.textDim);
    ui.TextField(Slate::Context::ID("name"), {x + 130, y, fieldW - 130, 32},
                 m_name, "Project name");
    y += 48.0f;

    ui.LabelIn({x, y, 120, 28}, "Directory", t.textDim);
    ui.TextField(Slate::Context::ID("dir"), {x + 130, y, fieldW - 130, 32},
                 m_directory, "Parent directory");
    y += 64.0f;

    ui.LabelIn({x, y, 400, 24}, "Choose a template", t.text);
    y += 34.0f;

    const auto& templates = Templates();
    f32 gap = 16.0f;
    f32 cardW = (fieldW - gap * 3) / 4.0f;
    f32 cardH = 150.0f;
    for (usize i = 0; i < templates.size(); ++i) {
        Rect card{x + static_cast<f32>(i) * (cardW + gap), y, cardW, cardH};
        bool selected = m_template == templates[i].value;
        if (ui.Card(Slate::Context::ID(templates[i].title), card,
                    templates[i].title, templates[i].desc, selected)) {
            m_template = templates[i].value;
        }
    }
    y += cardH + 28.0f;

    ui.LabelIn({x, y, fieldW, 24},
               "Selecting a template initializes the project with default "
               "behaviors.",
               t.textDim);

    // Create button, bottom-right of content.
    Rect createBtn{content.Right() - 190, content.Bottom() - 52, 166, 38};
    if (ui.Button(Slate::Context::ID("create"), createBtn, "Create Project")) {
        ProjectDesc desc;
        desc.name = m_name;
        desc.parentDirectory = m_directory;
        desc.gameTemplate = m_template;
        std::string err;
        if (auto project = Project::Create(desc, &err)) {
            result.launch = true;
            result.projectFile = project->ProjectFile();
            m_status = "Created '" + project->Name() + "'";
        } else {
            m_status = err;
        }
    }
}

void ProjectBrowser::DrawYourProjects(Slate::Context& ui, const Rect& content,
                                      BrowserResult& result) {
    Slate::Theme& t = ui.theme();
    f32 x = content.x + 16.0f;
    f32 y = content.y + 16.0f;
    f32 rowW = content.w - 32.0f;

    if (m_projects.empty()) {
        ui.LabelIn({content.x, content.y, content.w, content.h},
                   "No projects yet - create one in the New Project tab.",
                   t.textDim, Align::Center);
        return;
    }

    for (const ProjectEntry& entry : m_projects) {
        Rect row{x, y, rowW, 56};
        bool hovered = row.Contains(ui.mouse());
        ui.PanelBordered(row, hovered ? t.cardHover : t.cardBg, t.panelBorder);
        ui.LabelIn({row.x + 16, row.y, rowW - 200, 56}, entry.name, t.text);
        ui.LabelIn({row.x + 16, row.y + 26, rowW - 200, 26},
                   entry.file.string(), t.textDim);
        Rect openBtn{row.Right() - 110, row.y + 10, 96, 36};
        if (ui.Button(Slate::Context::ID(entry.file.string()), openBtn,
                      "Open")) {
            result.launch = true;
            result.projectFile = entry.file;
        }
        y += 64.0f;
    }
}

void ProjectBrowser::DrawAbout(Slate::Context& ui, const Rect& content) {
    Slate::Theme& t = ui.theme();
    f32 x = content.x + 24.0f;
    f32 y = content.y + 24.0f;
    ui.LabelIn({x, y, content.w, 36}, "Luma Engine", t.text, Align::Left, true);
    y += 48.0f;
    ui.LabelIn({x, y, content.w, 24},
               "A production-grade Vulkan game engine.", t.textDim);
    y += 30.0f;
    ui.LabelIn({x, y, content.w, 24},
               "Project browser and editor built with Luma Slate, our own UI "
               "framework.",
               t.textDim);
}

}  // namespace Luma
