#include "EditorScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include "Luma/Core/Log.h"
#include "Luma/Scene/Components.h"
#include "Luma/Scene/SceneSerializer.h"

namespace Luma {
using Slate::Align;
using Slate::Color;
using Slate::Rect;
using Slate::Vec2;

namespace {
// Default colors cycled for newly created entities.
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
    if (!LoadScene()) CreateEnvironment();

    // Wire the content browser to an asset registry rooted at the project's
    // Content/ folder. The panel reads from this; the registry is owned
    // here so we can call Scan() on file-watcher events.
    if (m_project) {
        auto contentRoot = m_project->ContentDir();
        std::error_code ec;
        if (std::filesystem::exists(contentRoot, ec)) {
            m_assetRegistry.AddRoot(contentRoot);
            m_assetRegistry.Scan();
            LUMA_LOG_INFO("Editor", "content root: {} ({} entries)",
                          contentRoot.string(), m_assetRegistry.Size());
        }
        m_contentBrowser.SetRegistry(&m_assetRegistry);
    }
}

void EditorScreen::CreateEnvironment() {
    m_environment = m_scene.CreateEntity("Environment");
    m_scene.Registry().emplace<EnvironmentComponent>(m_environment);
}

bool EditorScreen::LoadScene() {
    if (!m_project) return false;
    std::filesystem::path path = m_project->StartupScenePath();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return false;
    std::string err;
    if (!SceneSerializer::LoadFromFile(m_scene, path, &err)) {
        LUMA_LOG_ERROR("Editor", "failed to load scene '{}': {}", path.string(),
                       err);
        return false;
    }
    m_selected = kNullEntity;
    m_outlinerPanel.ClearSelection();
    m_inspectorPanel.ClosePopups();
    auto envView = m_scene.Registry().view<EnvironmentComponent>();
    if (envView.begin() == envView.end()) CreateEnvironment();
    LUMA_LOG_INFO("Editor", "loaded scene '{}'", path.string());
    return true;
}

void EditorScreen::SaveScene() {
    if (!m_project) return;
    std::filesystem::path path = m_project->StartupScenePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::string err;
    if (SceneSerializer::SaveToFile(m_scene, path, &err)) {
        LUMA_LOG_INFO("Editor", "saved scene '{}'", path.string());
    } else {
        LUMA_LOG_ERROR("Editor", "failed to save scene '{}': {}", path.string(),
                       err);
    }
}

void EditorScreen::AddEntity() {
    int index = m_nextNumber++;
    Entity e = m_scene.CreateEntity("Cube " + std::to_string(index));
    auto& mesh = m_scene.Registry().emplace<MeshRendererComponent>(e);
    mesh.albedo =
        kEntityPalette[(index - 1) % (sizeof(kEntityPalette) / sizeof(Math::Vec3))];
    auto& tf = m_scene.Registry().get<TransformComponent>(e);
    tf.position = Math::Vec3(static_cast<f32>(index - 1) * 2.0f, 1.0f, 0.0f);
    m_selected = e;
}

Editor::Panels::PanelContext EditorScreen::BuildPanelContext() {
    Editor::Panels::PanelContext ctx;
    ctx.scene = &m_scene;
    ctx.selected = &m_selected;
    ctx.camYaw = &m_camYaw;
    ctx.camPitch = &m_camPitch;
    ctx.camDistance = &m_camDistance;
    ctx.camTarget = &m_camTarget;
    ctx.fovY = m_fovY;
    ctx.nearZ = m_nearZ;
    ctx.farZ = m_farZ;
    ctx.view = &m_view;
    ctx.gizmoScale = &m_gizmoScale;
    ctx.gizmo = &m_gizmo;
    ctx.onAddEntity = [this] { AddEntity(); };
    ctx.onSaveScene = [this] { SaveScene(); };
    return ctx;
}

SceneView EditorScreen::BuildSceneView() {
    using namespace Math;
    Vec3 eye{m_camTarget.x + m_camDistance * std::cos(m_camPitch) *
                              std::sin(m_camYaw),
             m_camTarget.y + m_camDistance * std::sin(m_camPitch),
             m_camTarget.z + m_camDistance * std::cos(m_camPitch) *
                              std::cos(m_camYaw)};
    m_view = LookAt(eye, m_camTarget, Vec3(0.0f, 1.0f, 0.0f));
    m_gizmoScale = m_camDistance * 0.14f;

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
        scene.lighting.sunDirection = env.sunDirection;
        scene.lighting.sunColor = env.sunColor;
        scene.lighting.sunIntensity = env.sunIntensity * 3.0f;
        scene.lighting.groundColor = env.groundColor;
        scene.lighting.iblIntensity = env.skyIntensity;
        break;
    }

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

    if (m_scene.IsValid(m_selected) &&
        m_scene.Registry().all_of<TransformComponent>(m_selected)) {
        const auto& tf = m_scene.Registry().get<TransformComponent>(m_selected);
        const auto& lines = m_gizmo.BuildLines(tf.position, m_gizmoScale);
        scene.overlayLines = lines.data();
        scene.overlayLineVertexCount = static_cast<u32>(lines.size());
    }
    return scene;
}

void EditorScreen::BuildDock() {
    m_dock.AddPanel("outliner", "World Outliner",
                    [this](Slate::Context& c, const Rect& r) {
                        auto ctx = BuildPanelContext();
                        m_outlinerPanel.Draw(c, r, ctx);
                    });
    m_dock.AddPanel("viewport", "Viewport",
                    [this](Slate::Context& c, const Rect& r) {
                        auto ctx = BuildPanelContext();
                        m_viewportPanel.Draw(c, r, ctx);
                    });
    m_dock.AddPanel("inspector", "Inspector",
                    [this](Slate::Context& c, const Rect& r) {
                        auto ctx = BuildPanelContext();
                        m_inspectorPanel.Draw(c, r, ctx);
                    });
    m_dock.AddPanel("console", "Console",
                    [this](Slate::Context& c, const Rect& r) {
                        auto ctx = BuildPanelContext();
                        m_consolePanel.Draw(c, r, ctx);
                    });
    m_dock.AddPanel("content", "Content Browser",
                    [this](Slate::Context& c, const Rect& r) {
                        auto ctx = BuildPanelContext();
                        m_contentBrowser.Draw(c, r, ctx);
                    });
    m_dock.DockRoot("viewport");
    m_dock.DockWith("console", "viewport", Slate::DockDir::Down, 0.28f);
    m_dock.DockWith("outliner", "viewport", Slate::DockDir::Left, 0.2f);
    m_dock.DockWith("inspector", "viewport", Slate::DockDir::Right, 0.24f);
    m_dock.DockWith("content", "outliner", Slate::DockDir::Down, 0.45f);
    m_dockBuilt = true;
}

void EditorScreen::Draw(Slate::Context& ui, f32 width, f32 height) {
    Slate::Theme& t = ui.theme();
    if (!m_dockBuilt) BuildDock();
    ui.Panel({0, 0, width, height}, t.windowBg);

    Rect menu{0, 0, width, 32};
    ui.GradientRect(menu, t.surface2, t.surface1);
    ui.Panel({0, 31, width, 1}, t.separator);

    if (m_iconLogo) {
        ui.Image(m_iconLogo, {8, 4, 24, 24});
    } else {
        ui.LogoMark({20, 16}, 9.0f);
    }
    f32 mx = 42.0f;

    const char* items[] = {"File", "Edit", "Assets", "Window", "Help"};
    for (const char* item : items) {
        f32 w = ui.uiFont().Measure(item).x + 22.0f;
        bool clicked = ui.MenuButton(Slate::Context::ID(item), {mx, 4, w, 24},
                                     item);
        if (item == items[0]) {
            m_fileMenuX = mx;
            if (clicked) m_showFileMenu = !m_showFileMenu;
        } else if (clicked) {
            m_showFileMenu = false;
        }
        mx += w + 2.0f;
    }

    std::string projName = m_project ? m_project->Name() : "(no project)";
    f32 nameW = ui.uiFont().Measure(projName).x;
    f32 pillW = nameW + 34.0f;
    Rect pill{width - pillW - 10.0f, 5, pillW, 22};
    ui.PanelRoundedBordered(pill, t.surface1, t.outline,
                            t.radius.pill, t.border.hairline);
    ui.PanelRounded({pill.x + 11.0f, pill.y + 7.0f, 7.0f, 7.0f}, t.accent, 3.5f);
    ui.Heading({pill.x + 24.0f, pill.y, nameW + 8.0f, pill.h}, projName, t.text);

    Rect toolbar{0, 32, width, 36};
    ui.GradientRect(toolbar, t.surface1, t.surface0);
    ui.Panel({0, 67, width, 1}, t.separator);
    ui.IconButton(Slate::Context::ID("play"), {width / 2 - 46, 35, 30, 28},
                  m_iconPlay);
    ui.IconButton(Slate::Context::ID("pause"), {width / 2 - 14, 35, 30, 28},
                  m_iconPause);
    ui.IconButton(Slate::Context::ID("stop"), {width / 2 + 18, 35, 30, 28},
                  m_iconStop);

    m_dock.Draw(ui, {0, 68.0f, width, height - 68.0f});

    if (m_showFileMenu) {
        f32 mw = 190.0f;
        f32 mx0 = m_fileMenuX;
        f32 my0 = 30.0f;
        ui.PanelRoundedBordered({mx0 - 4, my0 - 4, mw + 8, 34.0f},
                                t.surface4, t.accent, t.radius.md,
                                t.border.thick);
        if (ui.Button(Slate::Context::ID("file.save"), {mx0, my0, mw, 26},
                      "Save Scene")) {
            SaveScene();
            m_showFileMenu = false;
        }
    }
}

}  // namespace Luma
