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

void EditorScreen::AddEntity() {
    Entity e;
    e.name = "Entity " + std::to_string(m_nextEntityNumber++);
    // Place new entities along X so several are visible without overlap.
    e.position = Math::Vec3(static_cast<f32>(m_entities.size()) * 2.0f, 1.0f,
                            0.0f);
    m_entities.push_back(e);
    m_selected = static_cast<int>(m_entities.size()) - 1;
}

Math::Mat4 EditorScreen::EntityMatrix(const Entity& e) const {
    using namespace Math;
    return Translate(e.position) * RotateY(Radians(e.rotationDeg.y)) *
           RotateX(Radians(e.rotationDeg.x)) * RotateZ(Radians(e.rotationDeg.z)) *
           Scale(e.scale);
}

SceneView EditorScreen::BuildSceneView() {
    using namespace Math;
    // Camera position from orbit angles.
    Vec3 eye{
        m_camTarget.x + m_camDistance * std::cos(m_camPitch) * std::sin(m_camYaw),
        m_camTarget.y + m_camDistance * std::sin(m_camPitch),
        m_camTarget.z + m_camDistance * std::cos(m_camPitch) * std::cos(m_camYaw)};

    m_instances.clear();
    m_instances.reserve(m_entities.size());
    for (int i = 0; i < static_cast<int>(m_entities.size()); ++i) {
        SceneInstance inst;
        inst.model = EntityMatrix(m_entities[static_cast<usize>(i)]);
        Vec3 c = m_entities[static_cast<usize>(i)].color;
        if (i == m_selected) {
            // Brighten the selected entity so it reads as highlighted.
            c = Vec3(std::min(1.0f, c.x + 0.25f), std::min(1.0f, c.y + 0.25f),
                     std::min(1.0f, c.z + 0.25f));
        }
        inst.color = c;
        m_instances.push_back(inst);
    }

    SceneView scene;
    scene.view = LookAt(eye, m_camTarget, Vec3(0.0f, 1.0f, 0.0f));
    scene.instances = m_instances.data();
    scene.instanceCount = static_cast<u32>(m_instances.size());
    return scene;
}

void EditorScreen::UpdateCamera(Slate::Context& ui, const Rect& viewport) {
    if (!viewport.Contains(ui.mouse())) return;

    if (ui.isMouseDown(0)) {
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

    DrawOutliner(ui, left);

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

    DrawInspector(ui, right);

    // Console.
    Rect con = ui.PanelWithTitle(console, "Console");
    ui.LabelIn({con.x + 12, con.y + 8, con.w - 20, 22}, "Luma Editor ready.",
               t.textDim);
}

void EditorScreen::DrawOutliner(Slate::Context& ui, const Rect& rect) {
    Slate::Theme& t = ui.theme();
    Rect body = ui.PanelWithTitle(rect, "World Outliner");

    // Add button in the title area.
    if (ui.Button(Slate::Context::ID("outliner.add"),
                  {rect.Right() - 52, rect.y + 2, 44, 22}, "+ Add")) {
        AddEntity();
    }

    if (m_entities.empty()) {
        ui.LabelIn({body.x, body.y + 8, body.w, 24}, "  (empty scene)",
                   t.textDim);
        return;
    }
    f32 y = body.y + 6.0f;
    for (int i = 0; i < static_cast<int>(m_entities.size()); ++i) {
        Rect row{body.x + 4, y, body.w - 8, 24};
        if (ui.Selectable(Slate::Context::ID(m_entities[static_cast<usize>(i)]
                                                 .name.c_str()),
                          row, m_entities[static_cast<usize>(i)].name,
                          i == m_selected)) {
            m_selected = i;
        }
        y += 26.0f;
    }
}

void EditorScreen::DrawInspector(Slate::Context& ui, const Rect& rect) {
    Slate::Theme& t = ui.theme();
    Rect body = ui.PanelWithTitle(rect, "Inspector");

    if (m_selected < 0 || m_selected >= static_cast<int>(m_entities.size())) {
        ui.LabelIn({body.x, body.y + 8, body.w, 24}, "  No selection",
                   t.textDim);
        return;
    }
    const Entity& e = m_entities[static_cast<usize>(m_selected)];
    f32 x = body.x + 12.0f;
    f32 y = body.y + 10.0f;
    ui.LabelIn({x, y, body.w - 24, 24}, e.name, t.text);
    y += 30.0f;

    auto vecRow = [&](const char* label, const Math::Vec3& v) {
        ui.LabelIn({x, y, 90, 22}, label, t.textDim);
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%.2f  %.2f  %.2f", v.x, v.y, v.z);
        ui.LabelIn({x + 90, y, body.w - 90 - 24, 22}, buf, t.text);
        y += 26.0f;
    };
    vecRow("Position", e.position);
    vecRow("Rotation", e.rotationDeg);
    vecRow("Scale", e.scale);
}

}  // namespace Luma
