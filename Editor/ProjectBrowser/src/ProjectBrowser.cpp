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

void ProjectBrowser::CreateFromInputs(BrowserResult& result) {
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

BrowserResult ProjectBrowser::Draw(Slate::Context& ui, f32 width, f32 height) {
    if (!m_scanned) Rescan();
    BrowserResult result;
    Slate::Theme& t = ui.theme();

    ui.Panel({0, 0, width, height}, t.windowBg);

    // Banner: subtle gradient with the Luma logo (image if available).
    Rect banner{0, 0, width, 84};
    ui.GradientRect(banner, Color::RGB(28, 33, 46), Color::RGB(18, 20, 26));
    if (m_logo.Valid()) {
        f32 h = 56.0f;
        f32 w = h * m_logo.ContentAspect();
        ui.ImageUV(m_logo.texture, {24, (84 - h) * 0.5f, w, h},
                   m_logo.contentUV);
    } else {
        ui.LogoMark({40, 42}, 20.0f);
        ui.LabelIn({70, 12, 320, 42}, "LUMA", t.accentText, Align::Left, true);
        ui.LabelIn({72, 52, 320, 20}, "ENGINE", t.textDim, Align::Left);
    }
    ui.LabelIn({0, 0, width - 24, 84}, "Project Browser", t.textDim,
               Align::Right);

    // Tabs.
    const char* tabNames[] = {"Your Projects", "New Project", "About"};
    const f32 tabW = 150.0f;
    const f32 tabY = 84.0f;
    for (int i = 0; i < 3; ++i) {
        Rect r{24 + i * tabW, tabY, tabW, 40};
        if (ui.Tab(Slate::Context::ID(tabNames[i]), r, tabNames[i],
                   m_tab == i)) {
            m_tab = i;
        }
    }
    // Thin divider under the tab row instead of a heavy content box.
    ui.Panel({0, tabY + 40, width, 1}, t.panelBorder);

    const f32 footerH = 60.0f;
    const f32 setupH = 34.0f;
    const f32 setupY = height - footerH - setupH;
    const f32 contentTop = tabY + 40.0f + 20.0f;
    // Content sits directly on the window background (no bordered box).
    Rect content{40, contentTop, width - 80, setupY - contentTop - 12.0f};

    if (m_tab == 0) DrawYourProjects(ui, content, result);
    else if (m_tab == 1) DrawNewProject(ui, content, result);
    else DrawAbout(ui, content);

    // Setup Options (collapsible) + expanded panel drawn as an overlay.
    if (m_setupOpen) {
        Rect panel{0, setupY - 122, width, 122};
        ui.Panel(panel, t.header);
        ui.PanelBordered(panel, Color::RGBA(0, 0, 0, 0), t.panelBorder);
        f32 ox = 32.0f;
        f32 oy = panel.y + 18.0f;
        ui.Checkbox(Slate::Context::ID("vsync"), {ox, oy, 18, 18},
                    "Enable VSync", m_vsync);
        oy += 30.0f;
        ui.Checkbox(Slate::Context::ID("git"), {ox, oy, 18, 18},
                    "Initialize Git repository", m_gitInit);
        oy += 30.0f;
        ui.Checkbox(Slate::Context::ID("starter"), {ox, oy, 18, 18},
                    "Generate starter content", m_starterContent);
    }
    ui.CollapsingHeader(Slate::Context::ID("setup"),
                        {0, setupY, width, setupH}, "Setup Options",
                        m_setupOpen);

    // Footer: Options (left), status (center), Create Project (right).
    Rect footer{0, height - footerH, width, footerH};
    ui.Panel(footer, t.header);
    ui.Button(Slate::Context::ID("options"),
              {24, height - footerH + 11, 130, 38}, "Options...");
    if (!m_status.empty()) {
        ui.LabelIn({170, height - footerH, width - 380, footerH}, m_status,
                   m_status.rfind("Created", 0) == 0 ? t.textDim
                                                     : Color::RGB(220, 120, 120),
                   Align::Left);
    }
    if (ui.Button(Slate::Context::ID("create"),
                  {width - 190, height - footerH + 11, 166, 38},
                  "Create Project")) {
        CreateFromInputs(result);
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
    f32 cardH = cardW / 1.5f;  // template thumbnails are ~3:2
    for (usize i = 0; i < templates.size(); ++i) {
        Rect card{x + static_cast<f32>(i) * (cardW + gap), y, cardW, cardH};
        bool selected = m_template == templates[i].value;
        const Slate::Image& thumb =
            m_thumbnails[static_cast<usize>(templates[i].value)];
        bool clicked =
            thumb.Valid()
                ? ui.ImageCard(Slate::Context::ID(templates[i].title), card,
                               thumb.texture, selected)
                : ui.Card(Slate::Context::ID(templates[i].title), card,
                          templates[i].title, templates[i].desc, selected);
        if (clicked) m_template = templates[i].value;
    }
    y += cardH + 28.0f;

    ui.LabelIn({x, y, fieldW, 24},
               "Selecting a template initializes the project with default "
               "behaviors.",
               t.textDim);
    LUMA_UNUSED(result);
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
        ui.PanelRoundedBordered(row, hovered ? t.cardHover : t.cardBg,
                                t.panelBorder, t.rounding);
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
