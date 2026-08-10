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

    f32 top = 68.0f;
    f32 bottomH = 180.0f;
    f32 leftW = 260.0f;
    f32 rightW = 300.0f;
    f32 centerH = height - top - bottomH;

    // World Outliner (left).
    Rect outliner{0, top, leftW, centerH};
    ui.PanelBordered(outliner, t.panelBg, t.panelBorder);
    ui.LabelIn({outliner.x, top, leftW, 30}, "  World Outliner", t.text);
    ui.LabelIn({outliner.x + 16, top + 40, leftW - 24, 24}, "Scene", t.textDim);

    // Viewport (center).
    Rect viewport{leftW, top, width - leftW - rightW, centerH};
    ui.Panel(viewport, Color::RGB(18, 20, 24));
    ui.PanelBordered({viewport.x, viewport.y, viewport.w, viewport.h},
                     Color::RGBA(0, 0, 0, 0), t.panelBorder);
    ui.LabelIn(viewport, "3D Viewport", t.textDim, Align::Center);

    // Inspector (right).
    Rect inspector{width - rightW, top, rightW, centerH};
    ui.PanelBordered(inspector, t.panelBg, t.panelBorder);
    ui.LabelIn({inspector.x, top, rightW, 30}, "  Inspector", t.text);
    if (m_project) {
        ui.LabelIn({inspector.x + 16, top + 44, rightW - 24, 24},
                   "Template: " + std::string(ToString(m_project->Template())),
                   t.textDim);
        ui.LabelIn({inspector.x + 16, top + 72, rightW - 24, 24},
                   "Engine: " + m_project->EngineVersion(), t.textDim);
    }

    // Console / Asset Browser (bottom).
    Rect bottom{0, height - bottomH, width, bottomH};
    ui.PanelBordered(bottom, t.panelBg, t.panelBorder);
    ui.LabelIn({bottom.x, bottom.y, 200, 30}, "  Console", t.text);
    ui.LabelIn({bottom.x + 16, bottom.y + 40, width - 24, 24},
               "Luma Editor ready.", t.textDim);
}

}  // namespace Luma
