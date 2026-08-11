#include "EditorScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Luma/Core/Log.h"

namespace Luma {
using Slate::Align;
using Slate::Color;
using Slate::Rect;
using Slate::Vec2;

namespace {
// Default colors cycled for newly created entities (a sensible default, not
// scene content - entities are created by the user).
const Math::Vec3 kEntityPalette[] = {
    {0.86f, 0.36f, 0.36f}, {0.40f, 0.72f, 0.92f}, {0.52f, 0.85f, 0.50f},
    {0.92f, 0.78f, 0.36f}, {0.74f, 0.52f, 0.92f}, {0.40f, 0.86f, 0.80f}};
}  // namespace

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
    int index = m_nextNumber++;
    Entity e = m_scene.CreateEntity("Cube " + std::to_string(index));
    auto& mesh = m_scene.Registry().emplace<MeshRendererComponent>(e);
    mesh.color = kEntityPalette[(index - 1) %
                                (sizeof(kEntityPalette) / sizeof(Math::Vec3))];
    // Space new entities out along X so they don't overlap.
    auto& tf = m_scene.Registry().get<TransformComponent>(e);
    tf.position = Math::Vec3(static_cast<f32>(index - 1) * 2.0f, 1.0f, 0.0f);
    m_selected = e;
}

SceneView EditorScreen::BuildSceneView() {
    using namespace Math;
    Vec3 eye{
        m_camTarget.x + m_camDistance * std::cos(m_camPitch) * std::sin(m_camYaw),
        m_camTarget.y + m_camDistance * std::sin(m_camPitch),
        m_camTarget.z + m_camDistance * std::cos(m_camPitch) * std::cos(m_camYaw)};
    m_view = LookAt(eye, m_camTarget, Vec3(0.0f, 1.0f, 0.0f));
    m_gizmoScale = m_camDistance * 0.14f;

    m_grid.Build(m_camTarget);

    // Entity instances.
    m_instances.clear();
    auto view = m_scene.Registry().view<TransformComponent, MeshRendererComponent>();
    for (Entity e : view) {
        SceneInstance inst;
        inst.model = view.get<TransformComponent>(e).Matrix();
        Vec3 c = view.get<MeshRendererComponent>(e).color;
        if (e == m_selected) {
            c = Vec3(std::min(1.0f, c.x + 0.25f), std::min(1.0f, c.y + 0.25f),
                     std::min(1.0f, c.z + 0.25f));
        }
        inst.color = c;
        m_instances.push_back(inst);
    }

    SceneView scene;
    scene.view = m_view;
    scene.fovYRadians = m_fovY;
    scene.nearZ = m_nearZ;
    scene.farZ = m_farZ;
    scene.lines = m_grid.Lines().data();
    scene.lineVertexCount = m_grid.VertexCount();
    scene.instances = m_instances.data();
    scene.instanceCount = static_cast<u32>(m_instances.size());

    // Gizmo overlay for the selected entity.
    if (m_scene.IsValid(m_selected) &&
        m_scene.Registry().all_of<TransformComponent>(m_selected)) {
        const auto& tf = m_scene.Registry().get<TransformComponent>(m_selected);
        const auto& lines = m_gizmo.BuildLines(tf.position, m_gizmoScale);
        scene.overlayLines = lines.data();
        scene.overlayLineVertexCount = static_cast<u32>(lines.size());
    }
    return scene;
}

void EditorScreen::UpdateCameraAndGizmo(Slate::Context& ui, const Rect& vp) {
    // Gizmo drag (left mouse) takes priority over camera when a selection exists.
    bool gizmoActive = false;
    if (m_scene.IsValid(m_selected) &&
        m_scene.Registry().all_of<TransformComponent>(m_selected)) {
        auto& tf = m_scene.Registry().get<TransformComponent>(m_selected);
        GizmoInput in;
        in.view = m_view;
        in.proj = Math::Perspective(m_fovY, vp.w / vp.h, m_nearZ, m_farZ);
        in.viewportX = vp.x;
        in.viewportY = vp.y;
        in.viewportW = vp.w;
        in.viewportH = vp.h;
        in.mouseX = ui.mouse().x;
        in.mouseY = ui.mouse().y;
        in.scale = m_gizmoScale;
        in.leftDown = vp.Contains(ui.mouse()) || m_gizmo.Dragging();
        in.leftDown = in.leftDown && ui.isMouseDown(0);
        in.leftPressed = vp.Contains(ui.mouse()) && ui.mousePressed(0);
        Math::Vec3 newPos;
        if (m_gizmo.Update(tf.position, in, newPos)) {
            tf.position = newPos;
        }
        gizmoActive = m_gizmo.Dragging();
    }

    if (gizmoActive || !vp.Contains(ui.mouse())) return;
    // Right-drag orbits; scroll zooms.
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

void EditorScreen::BuildDock() {
    m_dock.AddPanel("outliner", "World Outliner",
                    [this](Slate::Context& c, const Rect& r) {
                        DrawOutlinerContent(c, r);
                    });
    m_dock.AddPanel("viewport", "Viewport",
                    [this](Slate::Context& c, const Rect& r) {
                        DrawViewportContent(c, r);
                    });
    m_dock.AddPanel("inspector", "Inspector",
                    [this](Slate::Context& c, const Rect& r) {
                        DrawInspectorContent(c, r);
                    });
    m_dock.AddPanel("console", "Console",
                    [this](Slate::Context& c, const Rect& r) {
                        DrawConsoleContent(c, r);
                    });
    // Default layout: viewport center, console bottom (full width), outliner
    // left, inspector right.
    m_dock.DockRoot("viewport");
    m_dock.DockWith("console", "viewport", Slate::DockDir::Down, 0.28f);
    m_dock.DockWith("outliner", "viewport", Slate::DockDir::Left, 0.2f);
    m_dock.DockWith("inspector", "viewport", Slate::DockDir::Right, 0.24f);
    m_dockBuilt = true;
}

void EditorScreen::Draw(Slate::Context& ui, f32 width, f32 height) {
    Slate::Theme& t = ui.theme();
    if (!m_dockBuilt) BuildDock();
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

    // Toolbar.
    Rect toolbar{0, 32, width, 36};
    ui.Panel(toolbar, Color::RGB(28, 31, 37));
    ui.IconButton(Slate::Context::ID("play"), {width / 2 - 46, 35, 30, 28},
                  m_iconPlay);
    ui.IconButton(Slate::Context::ID("pause"), {width / 2 - 14, 35, 30, 28},
                  m_iconPause);
    ui.IconButton(Slate::Context::ID("stop"), {width / 2 + 18, 35, 30, 28},
                  m_iconStop);

    // Dockable panels fill the workspace below the toolbar.
    m_dock.Draw(ui, {0, 68.0f, width, height - 68.0f});
}

void EditorScreen::DrawViewportContent(Slate::Context& ui, const Rect& rect) {
    Slate::Theme& t = ui.theme();
    m_viewportRect = rect;
    if (m_viewport) {
        ui.Image(m_viewport, rect);
    } else {
        ui.Panel(rect, Color::RGB(18, 20, 24));
        ui.LabelIn(rect, "3D Viewport", t.textDim, Align::Center);
    }
    UpdateCameraAndGizmo(ui, rect);
}

void EditorScreen::DrawConsoleContent(Slate::Context& ui, const Rect& rect) {
    ui.LabelIn({rect.x + 12, rect.y + 8, rect.w - 20, 22}, "Luma Editor ready.",
               ui.theme().textDim);
}

void EditorScreen::DrawOutlinerContent(Slate::Context& ui, const Rect& body) {
    Slate::Theme& t = ui.theme();
    if (ui.Button(Slate::Context::ID("outliner.add"),
                  {body.Right() - 52, body.y + 4, 44, 22}, "+ Add")) {
        AddEntity();
    }

    auto view = m_scene.Registry().view<const NameComponent>();
    if (view.begin() == view.end()) {
        ui.LabelIn({body.x, body.y + 8, body.w, 22}, "  (no entities)",
                   t.textDim);
        return;
    }
    f32 y = body.y + 32.0f;  // below the +Add button
    for (Entity e : view) {
        const std::string& name = view.get<const NameComponent>(e).name;
        Rect row{body.x + 4, y, body.w - 8, 24};
        if (ui.Selectable(Slate::Context::ID(name.c_str()), row, name,
                          e == m_selected)) {
            m_selected = e;
        }
        y += 26.0f;
    }
}

void EditorScreen::DrawInspectorContent(Slate::Context& ui, const Rect& body) {
    Slate::Theme& t = ui.theme();

    if (!m_scene.IsValid(m_selected)) {
        ui.LabelIn({body.x, body.y + 8, body.w, 22}, "  No selection",
                   t.textDim);
        return;
    }
    auto& reg = m_scene.Registry();
    f32 x = body.x + 12.0f;
    f32 y = body.y + 12.0f;
    f32 w = body.w - 24.0f;
    ui.LabelIn({x, y, w, 22}, reg.get<NameComponent>(m_selected).name, t.text);
    y += 30.0f;

    // Transform section header.
    ui.Panel({body.x, y, body.w, 24}, t.header);
    ui.LabelIn({x, y, w, 24}, "Transform", t.text);
    y += 30.0f;

    auto& tf = reg.get<TransformComponent>(m_selected);
    auto field = [&](const char* label, u64 id, f32* v) {
        ui.LabelIn({x, y, 66, 24}, label, t.textDim);
        ui.Vector3Field(id, {x + 66, y, w - 66, 22}, v);
        y += 28.0f;
    };
    field("Position", Slate::Context::ID("insp.pos"), &tf.position.x);
    field("Rotation", Slate::Context::ID("insp.rot"), &tf.rotationEuler.x);
    field("Scale", Slate::Context::ID("insp.scl"), &tf.scale.x);
}

}  // namespace Luma
