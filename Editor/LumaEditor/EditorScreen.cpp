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

    // Every world starts with an Environment game object; its Environment
    // component (attached by default) drives the sky/atmosphere and lighting.
    CreateEnvironment();
}

void EditorScreen::CreateEnvironment() {
    m_environment = m_scene.CreateEntity("Environment");
    m_scene.Registry().emplace<EnvironmentComponent>(m_environment);
}

void EditorScreen::AddEntity() {
    int index = m_nextNumber++;
    Entity e = m_scene.CreateEntity("Cube " + std::to_string(index));
    auto& mesh = m_scene.Registry().emplace<MeshRendererComponent>(e);
    mesh.albedo = kEntityPalette[(index - 1) %
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

    // Entity instances.
    m_instances.clear();
    auto view = m_scene.Registry().view<TransformComponent, MeshRendererComponent>();
    for (Entity e : view) {
        const auto& mr = view.get<MeshRendererComponent>(e);
        SceneInstance inst;
        inst.model = view.get<TransformComponent>(e).Matrix();
        inst.primitive = mr.primitive;
        Vec3 c = mr.albedo;
        if (e == m_selected) {
            c = Vec3(std::min(1.0f, c.x + 0.20f), std::min(1.0f, c.y + 0.20f),
                     std::min(1.0f, c.z + 0.20f));
        }
        inst.albedo = c;
        inst.metallic = mr.metallic;
        inst.roughness = mr.roughness;
        m_instances.push_back(inst);
    }

    // Punctual lights (position from Transform, aim from its rotation).
    m_lights.clear();
    auto lightView =
        m_scene.Registry().view<TransformComponent, LightComponent>();
    for (Entity e : lightView) {
        const auto& tf = lightView.get<TransformComponent>(e);
        const auto& lc = lightView.get<LightComponent>(e);
        Mat4 mtx = tf.Matrix();
        Vec3 fwd = Normalize(Vec3(-mtx.m[8], -mtx.m[9], -mtx.m[10]));
        SceneLight sl;
        sl.type = static_cast<u32>(lc.type);
        sl.position = tf.position;
        sl.direction = fwd;
        sl.range = lc.range;
        sl.color = lc.color;
        sl.intensity = lc.intensity;
        sl.cosInner = std::cos(Radians(lc.innerAngleDeg));
        sl.cosOuter = std::cos(Radians(lc.outerAngleDeg));
        m_lights.push_back(sl);
    }

    SceneView scene;

    // Sky/atmosphere from the Environment component (first entity that has one).
    auto envView = m_scene.Registry().view<const EnvironmentComponent>();
    for (Entity e : envView) {
        const auto& env = envView.get<const EnvironmentComponent>(e);
        scene.sky.enabled = env.skyEnabled;
        scene.sky.sunDirection = env.sunDirection;
        scene.sky.groundColor = env.groundColor;
        scene.sky.turbidity = env.turbidity;
        scene.sky.sunIntensity = env.sunIntensity;
        scene.sky.skyIntensity = env.skyIntensity;
        scene.sky.sunSizeDegrees = env.sunSizeDegrees;

        // Same sun lights the meshes; sky drives the IBL ambient.
        scene.lighting.sunDirection = env.sunDirection;
        scene.lighting.sunColor = env.sunColor;
        scene.lighting.sunIntensity = env.sunIntensity * 3.0f;
        scene.lighting.groundColor = env.groundColor;
        scene.lighting.iblIntensity = env.skyIntensity;
        break;
    }

    // Infinite analytic ground grid; fade radius scales with the camera so it
    // always fills the view at any zoom (no fixed extent).
    scene.grid.enabled = true;
    scene.grid.cellSize = 1.0f;
    scene.grid.majorEvery = 10;
    scene.grid.fadeStart = m_camDistance * 3.0f;
    scene.grid.fadeEnd = m_camDistance * 60.0f;

    scene.view = m_view;
    scene.fovYRadians = m_fovY;
    scene.nearZ = m_nearZ;
    scene.farZ = m_farZ;
    scene.instances = m_instances.data();
    scene.instanceCount = static_cast<u32>(m_instances.size());
    scene.lights = m_lights.data();
    scene.lightCount = static_cast<u32>(m_lights.size());
    scene.lighting.shadowFocus = m_camTarget;  // keep shadows around the view

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

    // Menu bar: [gem] Luma  File Edit Assets Window Help ............ project.
    Rect menu{0, 0, width, 32};
    ui.GradientRect(menu, Color::RGB(46, 50, 60), Color::RGB(30, 33, 40));
    ui.Panel({0, 31, width, 1}, Color::RGB(12, 13, 16));

    // Brand: the Luma app icon (same footprint as Unreal's corner mark).
    if (m_iconLogo) {
        ui.Image(m_iconLogo, {8, 4, 24, 24});
    } else {
        ui.LogoMark({20, 16}, 9.0f);
    }
    f32 mx = 42.0f;

    const char* items[] = {"File", "Edit", "Assets", "Window", "Help"};
    for (const char* item : items) {
        f32 w = ui.uiFont().Measure(item).x + 22.0f;
        ui.MenuButton(Slate::Context::ID(item), {mx, 4, w, 24}, item);
        mx += w + 2.0f;
    }

    // Project identity pill on the right (accent dot + name).
    std::string projName = m_project ? m_project->Name() : "(no project)";
    f32 nameW = ui.uiFont().Measure(projName).x;
    f32 pillW = nameW + 34.0f;
    Rect pill{width - pillW - 10.0f, 5, pillW, 22};
    ui.PanelRoundedBordered(pill, Color::RGB(24, 26, 32),
                            Color::RGB(52, 57, 67), 11.0f, 1.0f);
    ui.PanelRounded({pill.x + 11.0f, pill.y + 7.0f, 7.0f, 7.0f}, t.accent, 3.5f);
    ui.Heading({pill.x + 24.0f, pill.y, nameW + 8.0f, pill.h}, projName, t.text);

    // Toolbar.
    Rect toolbar{0, 32, width, 36};
    ui.GradientRect(toolbar, Color::RGB(30, 33, 40), Color::RGB(22, 24, 29));
    ui.Panel({0, 67, width, 1}, t.panelBorder);
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
        m_showAddMenu = false;
        return;
    }
    auto& reg = m_scene.Registry();
    f32 x = body.x + 12.0f;
    f32 y = body.y + 12.0f;
    f32 w = body.w - 24.0f;

    // Header row: entity name + Add Component.
    ui.Heading({x, y, w - 118.0f, 24}, reg.get<NameComponent>(m_selected).name,
               t.text);
    if (ui.Button(Slate::Context::ID("insp.addcomp"),
                  {body.Right() - 118.0f, y, 106, 24}, "+ Component")) {
        m_showAddMenu = !m_showAddMenu;
    }
    y += 34.0f;

    auto sectionHeader = [&](const char* label) {
        ui.GradientRect({body.x, y, body.w, 26}, Color::RGB(44, 48, 57),
                        Color::RGB(34, 37, 44));
        ui.Panel({body.x, y, 3, 26}, t.accent);
        ui.Heading({x, y, w, 26}, label, t.text);
        y += 32.0f;
    };
    auto vec3Row = [&](const char* label, u64 id, f32* v) {
        ui.LabelIn({x, y, 82, 22}, label, t.textDim);
        ui.Vector3Field(id, {x + 82, y, w - 82, 22}, v);
        y += 28.0f;
    };
    auto floatRow = [&](const char* label, u64 id, f32& v) {
        ui.LabelIn({x, y, 118, 22}, label, t.textDim);
        ui.DragFloat(id, {x + 118, y, w - 118, 22}, v, 0.02f);
        y += 28.0f;
    };

    if (reg.all_of<TransformComponent>(m_selected)) {
        sectionHeader("Transform");
        auto& tf = reg.get<TransformComponent>(m_selected);
        vec3Row("Position", Slate::Context::ID("insp.pos"), &tf.position.x);
        vec3Row("Rotation", Slate::Context::ID("insp.rot"), &tf.rotationEuler.x);
        vec3Row("Scale", Slate::Context::ID("insp.scl"), &tf.scale.x);
        y += 8.0f;
    }
    if (reg.all_of<MeshRendererComponent>(m_selected)) {
        sectionHeader("Mesh Renderer");
        auto& mr = reg.get<MeshRendererComponent>(m_selected);
        // Primitive shape selector (segmented tabs).
        ui.LabelIn({x, y, 82, 22}, "Shape", t.textDim);
        const char* shapes[4] = {"Cube", "Plane", "Sphere", "Cylinder"};
        f32 bw = (w - 82.0f) / 4.0f;
        for (int p = 0; p < 4; ++p) {
            Rect r{x + 82.0f + bw * static_cast<f32>(p), y, bw - 2.0f, 22};
            bool sel = static_cast<int>(mr.primitive) == p;
            if (ui.Tab(Slate::Context::ID(shapes[p]), r, shapes[p], sel)) {
                mr.primitive = static_cast<MeshPrimitive>(p);
            }
        }
        y += 28.0f;
        vec3Row("Albedo", Slate::Context::ID("insp.alb"), &mr.albedo.x);
        floatRow("Metallic", Slate::Context::ID("insp.met"), mr.metallic);
        floatRow("Roughness", Slate::Context::ID("insp.rgh"), mr.roughness);
        y += 8.0f;
    }
    if (reg.all_of<EnvironmentComponent>(m_selected)) {
        sectionHeader("Environment");
        auto& env = reg.get<EnvironmentComponent>(m_selected);
        ui.Checkbox(Slate::Context::ID("insp.skyon"), {x, y, 18, 18},
                    "Sky Enabled", env.skyEnabled);
        y += 26.0f;
        vec3Row("Sun Dir", Slate::Context::ID("insp.sundir"),
                &env.sunDirection.x);
        vec3Row("Sun Color", Slate::Context::ID("insp.suncol"),
                &env.sunColor.x);
        vec3Row("Ground", Slate::Context::ID("insp.ground"),
                &env.groundColor.x);
        floatRow("Turbidity", Slate::Context::ID("insp.turb"), env.turbidity);
        floatRow("Sun Intensity", Slate::Context::ID("insp.suni"),
                 env.sunIntensity);
        floatRow("Sky Intensity", Slate::Context::ID("insp.skyi"),
                 env.skyIntensity);
        floatRow("Sun Size", Slate::Context::ID("insp.suns"),
                 env.sunSizeDegrees);
        y += 8.0f;
    }
    if (reg.all_of<LightComponent>(m_selected)) {
        sectionHeader("Light");
        auto& lc = reg.get<LightComponent>(m_selected);
        ui.LabelIn({x, y, 82, 22}, "Type", t.textDim);
        const char* types[3] = {"Directional", "Point", "Spot"};
        f32 bw = (w - 82.0f) / 3.0f;
        for (int p = 0; p < 3; ++p) {
            Rect r{x + 82.0f + bw * static_cast<f32>(p), y, bw - 2.0f, 22};
            bool sel = static_cast<int>(lc.type) == p;
            if (ui.Tab(Slate::Context::ID(types[p]), r, types[p], sel)) {
                lc.type = static_cast<LightType>(p);
            }
        }
        y += 28.0f;
        vec3Row("Color", Slate::Context::ID("insp.lcol"), &lc.color.x);
        floatRow("Intensity", Slate::Context::ID("insp.lint"), lc.intensity);
        if (lc.type != LightType::Directional) {
            floatRow("Range", Slate::Context::ID("insp.lrng"), lc.range);
        }
        if (lc.type == LightType::Spot) {
            floatRow("Inner Angle", Slate::Context::ID("insp.lin"),
                     lc.innerAngleDeg);
            floatRow("Outer Angle", Slate::Context::ID("insp.lout"),
                     lc.outerAngleDeg);
        }
        y += 8.0f;
    }

    // Add Component popup: lists component types not yet on the entity.
    if (m_showAddMenu) {
        bool canMesh = !reg.all_of<MeshRendererComponent>(m_selected);
        bool canEnv = !reg.all_of<EnvironmentComponent>(m_selected);
        bool canLight = !reg.all_of<LightComponent>(m_selected);
        int count = (canMesh ? 1 : 0) + (canEnv ? 1 : 0) + (canLight ? 1 : 0);
        f32 mw = 190.0f;
        f32 mx = body.Right() - mw - 12.0f;
        f32 my = body.y + 44.0f;
        f32 rows = static_cast<f32>(count > 0 ? count : 1);
        ui.PanelRoundedBordered({mx - 6, my - 6, mw + 12, rows * 28.0f + 12.0f},
                                Color::RGB(26, 28, 34), t.accent, 8.0f, 1.0f);
        if (canMesh &&
            ui.Button(Slate::Context::ID("add.mesh"), {mx, my, mw, 24},
                      "Mesh Renderer")) {
            reg.emplace<MeshRendererComponent>(m_selected);
            m_showAddMenu = false;
        }
        if (canMesh) my += 28.0f;
        if (canLight &&
            ui.Button(Slate::Context::ID("add.light"), {mx, my, mw, 24},
                      "Light")) {
            reg.emplace<LightComponent>(m_selected);
            m_showAddMenu = false;
        }
        if (canLight) my += 28.0f;
        if (canEnv &&
            ui.Button(Slate::Context::ID("add.env"), {mx, my, mw, 24},
                      "Environment")) {
            reg.emplace<EnvironmentComponent>(m_selected);
            m_showAddMenu = false;
        }
        if (count == 0) {
            ui.LabelIn({mx, my, mw, 24}, "  All components added", t.textDim);
        }
    }
}

}  // namespace Luma
