#include "EditorScreen.h"

#include <algorithm>
#include <cmath>

#include "Luma/Core/Log.h"

namespace Luma {
using Slate::Align;
using Slate::Color;
using Slate::Rect;
using Slate::Vec2;

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

SceneView EditorScreen::BuildSceneView() {
    using namespace Math;
    Vec3 eye{
        m_camTarget.x + m_camDistance * std::cos(m_camPitch) * std::sin(m_camYaw),
        m_camTarget.y + m_camDistance * std::sin(m_camPitch),
        m_camTarget.z + m_camDistance * std::cos(m_camPitch) * std::cos(m_camYaw)};

    // Infinite grid: recentre on the camera focus (fades with distance).
    m_grid.Build(m_camTarget);

    SceneView scene;
    scene.view = LookAt(eye, m_camTarget, Vec3(0.0f, 1.0f, 0.0f));
    scene.lines = m_grid.Lines().data();
    scene.lineVertexCount = m_grid.VertexCount();
    // Entities come from the ECS (EnTT) later; none rendered yet.
    scene.instances = nullptr;
    scene.instanceCount = 0;
    return scene;
}

void EditorScreen::UpdateCamera(Slate::Context& ui, const Rect& viewport) {
    if (!viewport.Contains(ui.mouse())) return;
    // Right-drag orbits (left is reserved for selection/gizmos later).
    if (ui.isMouseDown(1)) {
        Vec2 d = ui.mouseDelta();
        m_camYaw -= d.x * 0.01f;
        m_camPitch += d.y * 0.01f;
        m_camPitch = std::clamp(m_camPitch, -1.45f, 1.45f);
    }
    f32 scroll = ui.scrollDelta();
    if (scroll != 0.0f) {
        m_camDistance *= std::pow(0.9f, scroll);
        m_camDistance = std::clamp(m_camDistance, 2.0f, 60.0f);
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
    ui.LabelIn({width - 320, 0, 308, 32},
               m_project ? m_project->Name() : "(no project)", t.textDim,
               Align::Right);

    // Toolbar (play controls).
    Rect toolbar{0, 32, width, 36};
    ui.Panel(toolbar, Color::RGB(28, 31, 37));
    ui.IconButton(Slate::Context::ID("play"), {width / 2 - 46, 35, 30, 28},
                  m_iconPlay);
    ui.IconButton(Slate::Context::ID("pause"), {width / 2 - 14, 35, 30, 28},
                  m_iconPause);
    ui.IconButton(Slate::Context::ID("stop"), {width / 2 + 18, 35, 30, 28},
                  m_iconStop);

    // Dockable layout.
    f32 top = 68.0f;
    Rect workspace{0, top, width, height - top};
    Rect main, console;
    ui.SplitterH(Slate::Context::ID("dock.console"), workspace, m_consoleSplit,
                 main, console);
    Rect left, rest;
    ui.SplitterV(Slate::Context::ID("dock.left"), main, m_leftSplit, left, rest);
    Rect center, right;
    ui.SplitterV(Slate::Context::ID("dock.right"), rest, m_rightSplit, center,
                 right);

    // World Outliner (populated by the ECS later).
    Rect ob = ui.PanelWithTitle(left, "World Outliner");
    ui.LabelIn({ob.x, ob.y + 8, ob.w, 22}, "  (no entities)", t.textDim);

    // Viewport.
    Rect vp = ui.PanelWithTitle(center, "Viewport");
    m_viewportRect = vp;
    if (m_viewport) {
        ui.Image(m_viewport, vp);
    } else {
        ui.Panel(vp, Color::RGB(18, 20, 24));
        ui.LabelIn(vp, "3D Viewport", t.textDim, Align::Center);
    }
    UpdateCamera(ui, vp);

    // Inspector.
    Rect insp = ui.PanelWithTitle(right, "Inspector");
    ui.LabelIn({insp.x, insp.y + 8, insp.w, 22}, "  No selection", t.textDim);

    // Console.
    Rect con = ui.PanelWithTitle(console, "Console");
    ui.LabelIn({con.x + 12, con.y + 8, con.w - 20, 22}, "Luma Editor ready.",
               t.textDim);
}

}  // namespace Luma
