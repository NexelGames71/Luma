#include "EditorScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

#include "Luma/Asset/LumaMeshFormat.h"
#include "Luma/Asset/MeshImporter.h"
#include "Luma/Asset/TextureImporter.h"
#include "Luma/Asset/ThumbnailRenderer.h"
#include "Luma/Core/Log.h"
#include "Luma/Material/Material.h"
#include "Luma/Material/MaterialSerializer.h"
#include "Luma/Scene/Components.h"
#include "Luma/Scene/SceneSerializer.h"
#include "Luma/Renderer/DeferredShadingRenderer.h"

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

// Returns `base` unchanged when no entity in the scene shares that name;
// otherwise appends the lowest free number (base 1, base 2, ...) so the first
// of a kind stays unnumbered and duplicates get distinguished.
std::string UniqueEntityName(Luma::Scene& scene, const std::string& base) {
    auto view = scene.Registry().view<const NameComponent>();
    auto taken = [&view](const std::string& name) {
        for (Entity e : view) {
            if (view.get<const NameComponent>(e).name == name) return true;
        }
        return false;
    };
    if (!taken(base)) return base;
    int n = 1;
    while (taken(base + " " + std::to_string(n))) ++n;
    return base + " " + std::to_string(n);
}
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
    if (!LoadScene()) {
        CreateEnvironment();
        // New projects already declare a startup scene path. Materialize the
        // default environment scene on first open so the editor and runtime
        // operate on the same persisted scene instead of an in-memory fallback.
        SaveScene();
    }

    // Wire the content browser to an asset registry rooted at the project's
    // Assets/ folder (source assets live directly at the project root, not in
    // a Content/ subdirectory). The panel reads from this; the registry is
    // owned here so we can call Scan() on file-watcher events.
    if (m_project) {
        auto contentRoot = m_project->AssetsDir();
        std::error_code ec;
        if (std::filesystem::exists(contentRoot, ec)) {
            m_assetRegistry.AddRoot(contentRoot);
            m_assetRegistry.Scan();
            LUMA_LOG_INFO("Editor", "content root: {} ({} entries)",
                          contentRoot.string(), m_assetRegistry.Size());
        }
        m_contentBrowser.SetRegistry(&m_assetRegistry);

        // Route the thumbnail cache to <project>/Intermediate/Thumbnails so
        // thumbnails live in the project's scratch area, not inside Assets/.
        ThumbnailManager::Get().SetProjectRoot(m_project->RootDir());
        ThumbnailManager::Get().SetRegistry(&m_assetRegistry);
    }
    
    // Initialize deferred renderer
    m_deferredRenderer = Renderer2::CreateDeferredRenderer();
    if (m_deferredRenderer) {
        // Deferred renderer will be initialized later when RHI device is available
        LUMA_LOG_INFO("Editor", "Deferred renderer created (pending RHI device initialization)");
    }
}

void EditorScreen::InitializeDeferredRenderer(RHI::RHIDevice* device) {
    if (m_deferredRenderer && device) {
        if (m_deferredRenderer->Initialize(device)) {
            LUMA_LOG_INFO("Editor", "Deferred renderer initialized with RHI device");
        } else {
            LUMA_LOG_ERROR("Editor", "Failed to initialize deferred renderer");
        }
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

void EditorScreen::CaptureProjectPreview() {
    if (!m_project || !m_renderer) return;
    std::error_code ec;
    const std::filesystem::path dir =
        m_project->IntermediateDir() / "Thumbnails";
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        LUMA_LOG_WARN("Editor", "failed to create project preview directory: {}",
                      ec.message());
        return;
    }
    m_renderer->CaptureFrame((dir / "ScenePreview.png").string());
}

void EditorScreen::CreateActor(Editor::Panels::CreateActorKind kind) {
    int index = m_nextNumber++;
    const Math::Vec3 paletteColor = kEntityPalette[(index - 1) %
        (sizeof(kEntityPalette) / sizeof(Math::Vec3))];

    // --- Primitives / Empty -------------------------------------------------
    if (kind == Editor::Panels::CreateActorKind::Empty) {
        Entity e =
            m_scene.CreateEntity(UniqueEntityName(m_scene, "Empty Actor"));
        m_selected = e;
        return;
    }
    if (kind == Editor::Panels::CreateActorKind::Cube ||
        kind == Editor::Panels::CreateActorKind::Plane ||
        kind == Editor::Panels::CreateActorKind::Sphere ||
        kind == Editor::Panels::CreateActorKind::Cylinder) {
        const char* names[] = {"Cube", "Plane", "Sphere", "Cylinder"};
        const MeshPrimitive prims[] = {MeshPrimitive::Cube, MeshPrimitive::Plane,
                                       MeshPrimitive::Sphere,
                                       MeshPrimitive::Cylinder};
        int i = static_cast<int>(kind) - static_cast<int>(Editor::Panels::CreateActorKind::Cube);
        Entity e =
            m_scene.CreateEntity(UniqueEntityName(m_scene, names[i]));
        auto& mesh = m_scene.Registry().emplace<MeshRendererComponent>(e);
        mesh.primitive = prims[i];
        mesh.albedo = paletteColor;
        auto& tf = m_scene.Registry().get<TransformComponent>(e);
        tf.position = Math::Vec3(static_cast<f32>(index - 1) * 2.0f, 1.0f, 0.0f);
        m_selected = e;
        return;
    }

    // --- Lights -------------------------------------------------------------
    if (kind == Editor::Panels::CreateActorKind::DirectionalLight ||
        kind == Editor::Panels::CreateActorKind::PointLight ||
        kind == Editor::Panels::CreateActorKind::SpotLight ||
        kind == Editor::Panels::CreateActorKind::TubeLight) {
        const char* names[] = {"Directional Light", "Point Light",
                               "Spot Light", "Tube Light"};
        const LightType types[] = {LightType::Directional, LightType::Point,
                                   LightType::Spot, LightType::Tube};
        int i = static_cast<int>(kind) - static_cast<int>(Editor::Panels::CreateActorKind::DirectionalLight);
        Entity e =
            m_scene.CreateEntity(UniqueEntityName(m_scene, names[i]));
        auto& light = m_scene.Registry().emplace<LightComponent>(e);
        light.type = types[i];
        auto& tf = m_scene.Registry().get<TransformComponent>(e);
        // Lights sit above the origin; the direction comes from the transform's
        // rotation (default -Z forward, i.e. pointing at the scene).
        tf.position = Math::Vec3(static_cast<f32>(index - 1) * 3.0f, 4.0f, 2.0f);
        m_selected = e;
        return;
    }

    // --- Environment --------------------------------------------------------
    if (kind == Editor::Panels::CreateActorKind::Environment) {
        auto envView = m_scene.Registry().view<EnvironmentComponent>();
        if (envView.begin() == envView.end()) {
            Entity e = m_scene.CreateEntity("Environment");
            m_scene.Registry().emplace<EnvironmentComponent>(e);
            m_selected = e;
        } else {
            // Only one environment per scene — select the existing one.
            m_selected = *envView.begin();
        }
        return;
    }
}

Editor::Panels::PanelContext EditorScreen::BuildPanelContext() {
    Editor::Panels::PanelContext ctx;
    ctx.renderer = m_renderer;
    ctx.assetRegistry = &m_assetRegistry;
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
    ctx.onCreateActor = [this](Editor::Panels::CreateActorKind kind) {
        CreateActor(kind);
    };
    ctx.onSaveScene = [this] { SaveScene(); };
    ctx.onCreateMaterial = [this](const std::filesystem::path& folder) {
        return CreateMaterialAsset(folder);
    };
    ctx.onImportAssets = [this](const std::filesystem::path& folder) {
        ImportAssetsInto(folder);
    };
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
        
        // Load mesh asset if available
        if (mr.UsesMeshAsset() && m_project) {
            auto contentRoot = m_project->AssetsDir();
            auto assetEntry = m_assetRegistry.Lookup(mr.meshAsset);
            if (assetEntry) {
                auto meshPath = contentRoot / assetEntry->packagePath;
                
                // Check cache first
                auto it = m_meshCache.find(mr.meshAsset);
                if (it != m_meshCache.end()) {
                    // Use cached mesh data
                    const auto& meshData = it->second;
                    if (meshData.IsValid()) {
                        // Check if we have cached vertex positions
                        auto vertIt = m_vertexPositionCache.find(mr.meshAsset);
                        if (vertIt == m_vertexPositionCache.end()) {
                            // Extract and cache vertex positions
                            std::vector<Math::Vec3> positions;
                            positions.reserve(meshData.vertices.size());
                            for (const auto& v : meshData.vertices) {
                                positions.push_back(v.position);
                            }
                            m_vertexPositionCache[mr.meshAsset] = std::move(positions);
                            vertIt = m_vertexPositionCache.find(mr.meshAsset);
                        }
                        
                        // Validate data before using
                        if (vertIt->second.size() > 0 && vertIt->second.size() < 10000000) {
                            inst.customVertices = vertIt->second.data();
                            inst.customVertexCount = static_cast<u32>(vertIt->second.size());
                            inst.customIndices = meshData.indices.data();
                            inst.customIndexCount = static_cast<u32>(meshData.indices.size());
                            inst.customMeshValid = true;
                        } else {
                            LUMA_LOG_ERROR("Editor", "Invalid cached vertex data for asset");
                        }
                    }
                } else {
                    // Load and cache mesh
                    auto result = LumaMeshIO::ReadMesh(meshPath);
                    if (result && result->IsValid()) {
                        m_meshCache[mr.meshAsset] = std::move(*result);
                        const auto& cached = m_meshCache[mr.meshAsset];
                        
                        // Extract and cache vertex positions
                        std::vector<Math::Vec3> positions;
                        positions.reserve(cached.vertices.size());
                        for (const auto& v : cached.vertices) {
                            positions.push_back(v.position);
                        }
                        m_vertexPositionCache[mr.meshAsset] = std::move(positions);
                        
                        auto vertIt = m_vertexPositionCache.find(mr.meshAsset);
                        
                        // Validate data before using
                        if (vertIt->second.size() > 0 && vertIt->second.size() < 10000000) {
                            inst.customVertices = vertIt->second.data();
                            inst.customVertexCount = static_cast<u32>(vertIt->second.size());
                            inst.customIndices = cached.indices.data();
                            inst.customIndexCount = static_cast<u32>(cached.indices.size());
                            inst.customMeshValid = true;
                        } else {
                            LUMA_LOG_ERROR("Editor", "Invalid loaded vertex data for asset");
                        }
                    }
                }
            }
        }
        
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

    // The first directional light is the scene's sun: it drives the sky
    // atmosphere, the sun disk, cascaded shadows and the directional term in
    // every renderer. It is not added to the punctual list (it is rendered as
    // the sun instead). Point/spot lights (and any extra directional lights)
    // go into the punctual list.
    m_lights.clear();
    bool hasSun = false;
    SceneLight sun;
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
        sl.castShadows = lc.castShadows;
        sl.shadowBias = lc.shadowBias;
        sl.normalBias = lc.normalBias;
        sl.shadowSoftness = lc.shadowSoftness;
        sl.shadowMapSize = static_cast<u32>(lc.shadowMapSize);
        sl.cascadeCount = static_cast<u32>(lc.cascadeCount);
        sl.shadowDistance = lc.shadowDistance;
        sl.cascadeSplitLambda = lc.cascadeSplitLambda;
        sl.sunDiskSizeDeg = lc.sunDiskSizeDeg;
        sl.sunDiskIntensity = lc.sunDiskIntensity;
        if (lc.type == LightType::Directional && !hasSun) {
            // First directional light = sun (drives sky + lighting + shadows).
            hasSun = true;
            sun = sl;
            continue;
        }
        m_lights.push_back(sl);
    }

    SceneView scene;
    auto envView = m_scene.Registry().view<const EnvironmentComponent>();
    for (Entity e : envView) {
        const auto& env = envView.get<const EnvironmentComponent>(e);
        // --- Sky (physical atmosphere) ---
        scene.sky.enabled = env.skyEnabled;
        if (hasSun) {
            scene.sky.sunDirection = sun.direction;
            scene.sky.sunColor = sun.color;
            scene.sky.sunIntensity = sun.intensity;
            scene.sky.sunDiskSizeDeg = sun.sunDiskSizeDeg;
            scene.sky.sunDiskIntensity = sun.sunDiskIntensity;
        }
        scene.sky.rayleighScattering = env.rayleighScattering;
        scene.sky.rayleighScaleHeight = env.rayleighScaleHeight;
        scene.sky.mieScattering = env.mieScattering;
        scene.sky.mieAbsorption = env.mieAbsorption;
        scene.sky.mieScaleHeight = env.mieScaleHeight;
        scene.sky.mieAnisotropy = env.mieAnisotropy;
        scene.sky.ozoneScale = env.ozoneScale;
        scene.sky.skyIntensity = env.skyIntensity;
        scene.sky.saturation = env.saturation;
        scene.sky.exposure = env.exposure;
        scene.sky.skyTint = env.skyTint;
        scene.sky.groundColor = env.groundColor;
        // --- Lighting (sun + IBL ambient) ---
        if (hasSun) {
            scene.lighting.sunDirection = sun.direction;
            scene.lighting.sunColor = sun.color;
            scene.lighting.sunIntensity = sun.intensity * 3.0f;
            scene.lighting.sunShadows = sun.castShadows;
            scene.lighting.shadowDistance = sun.shadowDistance;
            scene.lighting.shadowSoftness = sun.shadowSoftness;
            scene.lighting.shadowBias = sun.shadowBias;
            scene.lighting.normalBias = sun.normalBias;
            scene.lighting.cascadeSplitLambda = sun.cascadeSplitLambda;
            scene.lighting.cascadeCount = sun.cascadeCount;
        }
        scene.lighting.groundColor = env.groundColor;
        scene.lighting.iblIntensity = env.iblIntensity;
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
    // The Material Editor is registered but NOT docked at startup — it is
    // docked lazily on the first material open (see Draw) so it never
    // occupies space when unused.
    m_dock.AddPanel("material", "Material Editor",
                    [this](Slate::Context& c, const Rect& r) {
                        auto ctx = BuildPanelContext();
                        m_materialEditor.Draw(c, r, ctx);
                    });
    m_dock.DockRoot("viewport");
    m_dock.DockWith("console", "viewport", Slate::DockDir::Down, 0.28f);
    m_dock.DockWith("outliner", "viewport", Slate::DockDir::Left, 0.2f);
    m_dock.DockWith("inspector", "viewport", Slate::DockDir::Right, 0.24f);
    m_dock.DockWith("content", "outliner", Slate::DockDir::Down, 0.45f);
    m_dockBuilt = true;
}

Renderer2::DeferredSceneView EditorScreen::BuildDeferredSceneView() {
    using namespace Math;
    
    // Build the RHI scene view first. Store it in a member so its instance /
    // light pointers stay valid for the lifetime of the returned view.
    m_deferredSceneSource = BuildSceneView();
    const SceneView& rhiScene = m_deferredSceneSource;
    
    // Convert to Renderer2 DeferredSceneView
    Renderer2::DeferredSceneView deferredScene;
    deferredScene.cameraPosition = Vec3(m_camTarget.x + m_camDistance * std::cos(m_camPitch) * std::sin(m_camYaw),
                                       m_camTarget.y + m_camDistance * std::sin(m_camPitch),
                                       m_camTarget.z + m_camDistance * std::cos(m_camPitch) * std::cos(m_camYaw));
    deferredScene.cameraDirection = Normalize(m_camTarget - deferredScene.cameraPosition);
    deferredScene.viewMatrix = m_view;
    
    // Use the engine-wide Vulkan projection convention, including its Y flip.
    // The actual viewport aspect is applied by the render loop once its rect
    // is known.
    deferredScene.projectionMatrix =
        Perspective(m_fovY, 1920.0f / 1080.0f, m_nearZ, m_farZ);
    
    deferredScene.viewProjectionMatrix = deferredScene.projectionMatrix * deferredScene.viewMatrix;
    deferredScene.fov = m_fovY;
    deferredScene.nearPlane = m_nearZ;
    deferredScene.farPlane = m_farZ;
    deferredScene.width = 1920; // Will be updated based on viewport
    deferredScene.height = 1080; // Will be updated based on viewport
    deferredScene.time = 0.0f; // TODO: Pass actual time
    deferredScene.deltaTime = 0.0f; // TODO: Pass actual delta time
    
    // Set pointers to RHI scene data
    deferredScene.sceneData = &rhiScene;
    deferredScene.lightingParams = &rhiScene.lighting;
    
    return deferredScene;
}

void EditorScreen::SetRenderMode(Renderer2::EditorRenderMode mode) {
    if (m_deferredRenderer) {
        m_deferredRenderer->SetEditorRenderMode(mode);
        LUMA_LOG_INFO("Editor", "Render mode set to {}", static_cast<int>(mode));
    }
}

Renderer2::EditorRenderMode EditorScreen::GetRenderMode() const {
    if (m_deferredRenderer) {
        return m_deferredRenderer->GetEditorRenderMode();
    }
    return Renderer2::EditorRenderMode::Deferred;
}

void EditorScreen::ToggleDebugVisualization() {
    if (m_deferredRenderer) {
        bool current = m_deferredRenderer->GetDebugVisualization();
        m_deferredRenderer->SetDebugVisualization(!current);
        LUMA_LOG_INFO("Editor", "Debug visualization {}", !current ? "enabled" : "disabled");
    }
}

bool EditorScreen::GetDebugVisualization() const {
    if (m_deferredRenderer) {
        return m_deferredRenderer->GetDebugVisualization();
    }
    return false;
}

std::filesystem::path EditorScreen::CreateMaterialAsset(
    const std::filesystem::path& folder) {
    if (!m_project) return {};
    std::filesystem::path target = folder;
    if (target.empty()) target = m_project->AssetsDir();
    std::error_code ec;
    std::filesystem::create_directories(target, ec);

    // Unique name: NewMaterial, NewMaterial 1, NewMaterial 2, ...
    std::filesystem::path path = target / "NewMaterial.lmat";
    int n = 1;
    while (std::filesystem::exists(path, ec)) {
        path = target / ("NewMaterial " + std::to_string(n++) + ".lmat");
    }

    Luma::Material::Material mat;
    mat.name = path.stem().string();
    
    // Set Unreal Engine default material values
    mat.SetScalarValue(Luma::Material::MaterialProperty::Metallic, 0.0f);
    mat.SetScalarValue(Luma::Material::MaterialProperty::Roughness, 0.5f);
    mat.SetScalarValue(Luma::Material::MaterialProperty::Specular, 0.5f);
    mat.SetVectorValue(Luma::Material::MaterialProperty::BaseColor, {1.0f, 1.0f, 1.0f});
    
    std::string err;
    if (!Luma::Material::MaterialSerializer::SaveToFile(mat, path, &err)) {
        LUMA_LOG_ERROR("Editor", "failed to create material '{}': {}",
                       path.string(), err);
        return {};
    }
    LUMA_LOG_INFO("Editor", "created material '{}'", path.string());
    return path;
}

void EditorScreen::HandleDroppedFiles(const std::vector<std::filesystem::path>& files) {
    if (files.empty() || !m_project) return;

    std::filesystem::path targetDir = m_contentBrowser.CurrentFolder();
    if (targetDir.empty() || !std::filesystem::exists(targetDir)) {
        targetDir = m_project->AssetsDir();
    }
    std::error_code ec;
    std::filesystem::create_directories(targetDir, ec);

    LUMA_LOG_INFO("Editor", "Handling dropped assets/packs ({}) into target folder {}", files.size(), targetDir.string());

    auto processSingleFile = [&](const std::filesystem::path& srcPath, const std::filesystem::path& destDir) {
        std::error_code copyEc;
        std::filesystem::path destPath = destDir / srcPath.filename();

        // Copy source file to target directory if not already there
        if (std::filesystem::canonical(srcPath, copyEc) != std::filesystem::canonical(destPath, copyEc)) {
            std::filesystem::copy_file(srcPath, destPath, std::filesystem::copy_options::overwrite_existing, copyEc);
            if (copyEc) {
                LUMA_LOG_ERROR("Editor", "Failed to copy dropped file {} -> {}: {}", srcPath.string(), destPath.string(), copyEc.message());
                return;
            }
        }
        LUMA_LOG_INFO("Editor", "Copied dropped file to {}", destPath.string());
    };

    for (const auto& file : files) {
        if (std::filesystem::is_directory(file)) {
            // Recursive copy for directories
            std::filesystem::path destDir = targetDir / file.filename();
            std::error_code copyEc;
            std::filesystem::copy(file, destDir, std::filesystem::copy_options::recursive, copyEc);
            if (copyEc) {
                LUMA_LOG_ERROR("Editor", "Failed to copy dropped directory {} -> {}: {}", file.string(), destDir.string(), copyEc.message());
            } else {
                LUMA_LOG_INFO("Editor", "Copied dropped directory to {}", destDir.string());
            }
        } else {
            processSingleFile(file, targetDir);
        }
    }

    // Refresh the registry after dropping files
    m_assetRegistry.Scan();
    LUMA_LOG_INFO("Editor", "Registry refreshed after drop ({} entries)", m_assetRegistry.Size());
}

void EditorScreen::ImportAssetsInto(const std::filesystem::path& folder) {
    if (!m_project) return;
    std::filesystem::path target = folder;
    if (target.empty()) target = m_project->AssetsDir();
    std::error_code ec;
    std::filesystem::create_directories(target, ec);

    // Native multi-select file picker (Windows).
    std::vector<std::filesystem::path> picked;
#ifdef _WIN32
    {
        char buffer[16384] = {0};
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "All Files (*.*)\0*.*\0";
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = static_cast<DWORD>(sizeof(buffer));
        ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST;
        ofn.lpstrTitle = "Import Assets";
        if (GetOpenFileNameA(&ofn)) {
            // With OFN_ALLOWMULTISELECT + OFN_EXPLORER the buffer is
            // "dir\0file1\0file2\0\0" for several files, or a single full
            // path when exactly one file is chosen.
            const char* p = buffer;
            std::string first(p);
            p += first.size() + 1;
            if (*p == '\0') {
                picked.push_back(first);
            } else {
                const std::filesystem::path dir(first);
                while (*p != '\0') {
                    picked.push_back(dir / p);
                    p += std::strlen(p) + 1;
                }
            }
        }
    }
#endif
    if (picked.empty()) return;

    LUMA_LOG_INFO("Editor", "Importing {} file(s) into {}", picked.size(),
                  target.string());
    for (const auto& src : picked) {
        std::error_code copyEc;
        if (!std::filesystem::exists(src, copyEc) || copyEc) {
            continue;  // no longer exists — skip
        }
        std::filesystem::path dest = target / src.filename();
        copyEc.clear();
        std::filesystem::path srcReal = std::filesystem::canonical(src, copyEc);
        if (copyEc) continue;
        copyEc.clear();
        std::filesystem::path destReal =
            std::filesystem::canonical(dest, copyEc);
        if (!copyEc && srcReal == destReal) continue;  // already there
        copyEc.clear();
        std::filesystem::copy_file(src, dest,
                                   std::filesystem::copy_options::overwrite_existing,
                                   copyEc);
        if (copyEc) {
            LUMA_LOG_ERROR("Editor", "Failed to import {} -> {}: {}",
                           src.string(), dest.string(), copyEc.message());
        }
    }

    m_assetRegistry.Scan();
    LUMA_LOG_INFO("Editor", "Registry refreshed after import ({} entries)",
                  m_assetRegistry.Size());
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

    // Content Browser double-click: open the right editor for the activated
    // asset. Materials open the Material Editor (docked lazily on first use).
    {
        AssetId activated = m_contentBrowser.Activated();
        m_contentBrowser.ClearActivated();
        if (activated.IsValid()) {
            if (const AssetData* ad = m_assetRegistry.Lookup(activated)) {
                if (ad->type == AssetType::Material) {
                    if (!m_materialEditor.Open(ad->packagePath)) {
                        LUMA_LOG_ERROR("Editor",
                                       "failed to open material '{}'",
                                       ad->packagePath.string());
                    } else {
                        LUMA_LOG_INFO("Editor", "opened material '{}'",
                                      ad->packagePath.string());
                        if (!m_materialDocked) {
                            m_dock.DockWith("material", "viewport",
                                            Slate::DockDir::Right, 0.38f);
                            m_materialDocked = true;
                        }
                    }
                }
            }
        }
    }

    // Floating color pickers — drawn after the dock so the panels aren't
    // clipped to the inspector column (the dock scissor-clips every panel).
    {
        auto ctx = BuildPanelContext();
        m_inspectorPanel.DrawFloatingPickers(ui, ctx);
    }

    // Material Editor color-picker popups (swatches live on the graph nodes
    // and the Material Output node; the floating panels need the unwound
    // clip stack so they can overflow the editor panel's rect).
    m_materialEditor.DrawFloatingPickers(ui);

    // World Outliner create-menu overlay — drawn after the dock (same reason
    // as the floating pickers) so the menu renders on top of the panels
    // instead of being clipped to the outliner column.
    {
        auto ctx = BuildPanelContext();
        m_outlinerPanel.DrawFloatingMenu(ui, ctx);
    }

    // Content Browser right-click create-menu overlay — same floating pass
    // (the menu anchors at the cursor and can overhang the panel's rect).
    {
        auto ctx = BuildPanelContext();
        m_contentBrowser.DrawFloatingMenu(ui, ctx);
    }

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
