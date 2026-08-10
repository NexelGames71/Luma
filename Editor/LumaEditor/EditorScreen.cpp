#include "EditorScreen.h"

#include "Luma/Core/Log.h"

namespace Luma {
using Slate::Align;
using Slate::Color;
using Slate::Rect;

EditorScreen::EditorScreen(const std::filesystem::path& projectFile) {
    std::string err;
    m_project = Project::Load(projectFile, &err);
    if (m_project) {
        m_title = m_project->Name() + " - Luma Editor";
        LUMA_LOG_INFO("Editor", "opened project '{}'", m_project->Name());
    } else {
        m_title = "Luma Editor";
        LUMA_LOG_ERROR("Editor", "could not open project: {}", err);
    }
}

void EditorScreen::Draw(Slate::Context& ui, f32 width, f32 height) {
    Slate::Theme& t = ui.theme();
    ui.Panel({0, 0, width, height}, t.windowBg);

    // Menu bar.
    Rect menu{0, 0, width, 32};
    ui.Panel(menu, t.header);
    const char* items[] = {"Luma", "File", "Edit", "Assets", "Window", "Help"};
    f32 mx = 12.0f;
    for (const char* item : items) {
        f32 w = ui.font().Measure(item).x + 24.0f;
        ui.Button(Slate::Context::ID(item), {mx, 2, w, 28}, item);
        mx += w;
    }
    std::string name = m_project ? m_project->Name() : "(no project)";
    ui.LabelIn({width - 320, 0, 308, 32}, name, t.textDim, Align::Right);

    // Toolbar (play controls placeholder).
    Rect toolbar{0, 32, width, 36};
    ui.Panel(toolbar, Color::RGB(28, 31, 37));
    ui.Button(Slate::Context::ID("play"), {width / 2 - 40, 36, 36, 28}, ">");
    ui.Button(Slate::Context::ID("pause"), {width / 2, 36, 36, 28}, "||");
    ui.Button(Slate::Context::ID("stop"), {width / 2 + 40, 36, 36, 28}, "[]");

    // Dockable layout: split the workspace with draggable splitters.
    f32 top = 68.0f;
    Rect workspace{0, top, width, height - top};

    Rect main, console;
    ui.SplitterH(Slate::Context::ID("dock.console"), workspace, m_consoleSplit,
                 main, console);
    Rect left, rest;
    ui.SplitterV(Slate::Context::ID("dock.left"), main, m_leftSplit, left,
                 rest);
    Rect center, right;
    ui.SplitterV(Slate::Context::ID("dock.right"), rest, m_rightSplit, center,
                 right);

    // World Outliner.
    Rect ob = ui.PanelWithTitle(left, "World Outliner");
    ui.LabelIn({ob.x + 8, ob.y + 6, ob.w - 16, 22}, "Scene", t.textDim);

    // Viewport (center): show the rendered scene texture, or a placeholder.
    Rect vp = ui.PanelWithTitle(center, "Viewport");
    m_viewportRect = vp;
    if (m_viewport) {
        ui.Image(m_viewport, vp);
    } else {
        ui.Panel(vp, Color::RGB(18, 20, 24));
        ui.LabelIn(vp, "3D Viewport", t.textDim, Align::Center);
    }

    // Inspector.
    Rect insp = ui.PanelWithTitle(right, "Inspector");
    if (m_project) {
        ui.LabelIn({insp.x + 12, insp.y + 10, insp.w - 20, 22},
                   "Template: " + std::string(ToString(m_project->Template())),
                   t.textDim);
        ui.LabelIn({insp.x + 12, insp.y + 36, insp.w - 20, 22},
                   "Engine: " + m_project->EngineVersion(), t.textDim);
    }

    // Console.
    Rect con = ui.PanelWithTitle(console, "Console");
    ui.LabelIn({con.x + 12, con.y + 8, con.w - 20, 22}, "Luma Editor ready.",
               t.textDim);
}

}  // namespace Luma
