#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Asset/AssetImportManager.h"
#include "Luma/Asset/AssetFileWatcher.h"
#include "Luma/Asset/LumaMesh.h"
#include "Luma/Editor/Panels/Console.h"
#include "Luma/Editor/Panels/ContentBrowser.h"
#include "Luma/Editor/Panels/Inspector.h"
#include "Luma/Editor/Panels/MaterialEditor.h"
#include "Luma/Editor/Panels/Viewport.h"
#include "Luma/Editor/Panels/WorldOutliner.h"
#include "PanelContext.h"
#include "Luma/Gizmo/TranslateGizmo.h"
#include "Luma/Math/Math.h"
#include "Luma/Project/Project.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/RHI/RHIContext.h"
#include "Luma/Renderer/DeferredShadingRenderer.h"
#include "Luma/Scene/Scene.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/DockSpace.h"

namespace Luma {

// The editor shell: menu bar, toolbar, and a dockable layout (World Outliner,
// Viewport, Inspector, Console). Drives an ECS Scene (EnTT) and orchestrates
// the per-panel modules under Editor/panels/. The panels themselves are
// self-contained; this class only wires them together + owns the shared
// state (scene, camera, gizmo, selection).
class EditorScreen {
public:
    explicit EditorScreen(const std::filesystem::path& projectFile);

    bool HasProject() const { return m_project.has_value(); }

    // Builds the scene view (camera + grid + entity instances + gizmo overlay).
    SceneView BuildSceneView();

    void Draw(Slate::Context& ui, f32 width, f32 height);

    Slate::Rect ViewportRect() const { return m_viewportPanel.Rect(); }
    void SetViewportTexture(TextureHandle texture) {
        m_viewportPanel.SetTexture(texture);
    }

    void SetToolbarIcons(TextureHandle play, TextureHandle pause,
                         TextureHandle stop) {
        m_iconPlay = play;
        m_iconPause = pause;
        m_iconStop = stop;
    }
    void SetLogoIcon(TextureHandle logo) { m_iconLogo = logo; }

    // Handle dropped files from the OS (forwarded to Content Browser)
    void HandleDroppedFiles(const std::vector<std::filesystem::path>& files);

    // "Import to Current Folder" (Content Browser right-click menu): opens a
    // native multi-select file dialog, copies the picked files into `folder`
    // (empty = content root) and rescans the asset registry.
    void ImportAssetsInto(const std::filesystem::path& folder);

    // Wire the renderer for thumbnail generation
    void SetRenderer(Luma::Renderer* renderer) {
        m_renderer = renderer;
        m_contentBrowser.SetRenderer(renderer);
    }
    Luma::Renderer* Renderer() const noexcept { return m_renderer; }
    
    // Get deferred renderer
    Renderer2::DeferredShadingRenderer* GetDeferredRenderer() const { return m_deferredRenderer; }
    
    // Set deferred renderer
    void SetDeferredRenderer(Renderer2::DeferredShadingRenderer* renderer) { m_deferredRenderer = renderer; }
    
    // Initialize deferred renderer with RHI device
    void InitializeDeferredRenderer(RHI::RHIDevice* device);
    
    // Build scene view for deferred renderer
    Renderer2::DeferredSceneView BuildDeferredSceneView();
    
    // Editor rendering mode controls
    void SetRenderMode(Renderer2::EditorRenderMode mode);
    Renderer2::EditorRenderMode GetRenderMode() const;
    void ToggleDebugVisualization();
    bool GetDebugVisualization() const;

    // Forwards the Create-menu header button icon to the outliner panel.
    // A zero handle falls back to a procedural plus glyph.
    void SetCreateButtonIcon(TextureHandle icon) {
        m_outlinerPanel.SetCreateButtonIcon(icon);
    }

    // Forwards the Create-menu search-glass texture (from the ThirdParty
    // Unreal icon pack). A zero handle falls back to the procedural glyph.
    void SetSearchGlassIcon(TextureHandle icon) {
        m_outlinerPanel.SetSearchGlassIcon(icon);
    }

    // Forwards the Create-menu category icons (Geometry / Lights /
    // Environment — Unreal-style silhouettes, drawn gray in the menu).
    // Zero handles fall back to the procedural glyphs.
    void SetCategoryIcons(TextureHandle geometry, TextureHandle light,
                          TextureHandle environment) {
        m_outlinerPanel.SetCategoryIcons(geometry, light, environment);
    }

    // Forwards the primitive-actor icons for the Geometry submenu rows
    // (Unreal primitive thumbnails, drawn gray). Zero handles fall back to
    // the procedural glyphs.
    void SetPrimitiveIcons(TextureHandle cube, TextureHandle plane,
                           TextureHandle sphere, TextureHandle cylinder) {
        m_outlinerPanel.SetPrimitiveIcons(cube, plane, sphere, cylinder);
    }

    // Forwards the outliner actor icons (light types + generic mesh) shown
    // beside entity names in the World Outliner list. Zero handles fall back
    // to no icon.
    void SetOutlinerActorIcons(TextureHandle dirLight, TextureHandle pointLight,
                               TextureHandle spotLight, TextureHandle mesh) {
        m_outlinerPanel.SetOutlinerActorIcons(dirLight, pointLight, spotLight,
                                              mesh);
    }

    // Forwards Content Browser chrome icons to the panel. Missing textures
    // (any zero handle) fall back to procedural glyphs.
    void SetContentBrowserIcons(TextureHandle sortUp, TextureHandle sortDown,
                                TextureHandle searchGlass,
                                TextureHandle folder, TextureHandle reload,
                                TextureHandle importTex,
                                TextureHandle openFolder,
                                TextureHandle expandArrow,
                                TextureHandle retractArrow) {
        m_contentBrowser.SetIcons(sortUp, sortDown, searchGlass, folder,
                                   reload, importTex, openFolder, expandArrow,
                                   retractArrow);
    }

private:
    // Spawns a new game object from the World Outliner's Create menu
    // (Empty / primitives / Light > directional, point, spot, tube / Environment).
    void CreateActor(Editor::Panels::CreateActorKind kind);
    // Creates a new default .lmat material asset in `folder` (empty = content
    // root). Returns the created path (empty on failure).
    std::filesystem::path CreateMaterialAsset(
        const std::filesystem::path& folder);
    void CreateEnvironment();
    bool LoadScene();
    void SaveScene();
    void BuildDock();

    // ---- Shared state the panels read/write via PanelContext ----------------
    std::optional<Project> m_project;
    std::string m_title;

    Scene m_scene;
    Entity m_selected = kNullEntity;
    Entity m_environment = kNullEntity;
    int m_nextNumber = 1;

    TranslateGizmo m_gizmo;
    std::vector<SceneInstance> m_instances;
    std::vector<SceneLight> m_lights;

    f32 m_camYaw = 0.9f;
    f32 m_camPitch = 0.5f;
    f32 m_camDistance = 12.0f;
    Math::Vec3 m_camTarget{0.0f, 0.0f, 0.0f};

    Math::Mat4 m_view = Math::Mat4::Identity();
    f32 m_fovY = 0.9f;
    f32 m_nearZ = 0.1f;
    f32 m_farZ = 500.0f;
    f32 m_gizmoScale = 1.0f;

    Slate::DockSpace m_dock;
    bool m_dockBuilt = false;

    // Content Browser — wires to the project's Content/ folder so the
    // registry is populated before the panel is drawn each frame.
    AssetRegistry m_assetRegistry;
    
    // Cache for loaded mesh data (asset ID -> mesh data)
    std::unordered_map<AssetId, LumaMeshData> m_meshCache;
    
    // Cache for extracted vertex positions per asset (asset ID -> vertex positions)
    std::unordered_map<AssetId, std::vector<Math::Vec3>> m_vertexPositionCache;

    bool m_showFileMenu = false;
    f32 m_fileMenuX = 42.0f;

    TextureHandle m_iconPlay = 0;
    TextureHandle m_iconPause = 0;
    TextureHandle m_iconStop = 0;
    TextureHandle m_iconLogo = 0;

    // ---- Panels (one per docked pane) ---------------------------------------
    Editor::Panels::WorldOutlinerPanel m_outlinerPanel;
    Editor::Panels::ViewportPanel m_viewportPanel;
    Editor::Panels::InspectorPanel m_inspectorPanel;
    Editor::Panels::ConsolePanel m_consolePanel;
    Editor::Panels::ContentBrowserPanel m_contentBrowser;
    Editor::Panels::MaterialEditorPanel m_materialEditor;

    // Material editor docking: registered at startup, docked lazily the first
    // time a material is opened so it never takes space when unused.
    bool m_materialDocked = false;

    // Renderer for thumbnail generation
    Luma::Renderer* m_renderer = nullptr;
    
    // Deferred renderer for advanced rendering
    Renderer2::DeferredShadingRenderer* m_deferredRenderer = nullptr;

    // Holds the source SceneView for the deferred pipeline. Must outlive the
    // DeferredSceneView returned by BuildDeferredSceneView (its sceneData /
    // lightingParams members point into this).
    SceneView m_deferredSceneSource;

    Editor::Panels::PanelContext BuildPanelContext();
};

}  // namespace Luma
