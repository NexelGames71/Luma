#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Luma/Core/EngineLoop.h"
#include "Luma/Core/Events.h"
#include "Luma/Core/Log.h"
#include "Luma/Core/Types.h"
#include "Luma/Editor/ProjectBrowser.h"
#include "Luma/Platform/Process.h"
#include "Luma/Platform/Window.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/RHI/VulkanRenderer.h"
#include "Luma/RHI/VulkanRHIDevice.h"
#include "Luma/Rendering/Vulkan/VulkanDeferredRenderer.h"
#include "Luma/Renderer/DeferredShadingRenderer.h"
#include "Luma/Renderer/Lighting.h"
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Image.h"
#include "Luma/VFS/Path.h"
#include "Luma/VFS/VFS.h"
#include "Luma/Asset/ThumbnailRenderer.h"
#include "Luma/Asset/TextureThumbnailRenderer.h"
#include "Luma/Asset/MeshThumbnailRenderer.h"
#include "Luma/Asset/MaterialThumbnailRenderer.h"

#include "EditorScreen.h"
#include "SplashScreen.h"

#include <vulkan/vulkan.h>

using namespace Luma;

namespace {

// Resolves an editor asset (e.g. "Fonts/Inter-Regular.ttf") through the VFS
// using the Content root. Returns the absolute filesystem path or an empty
// string if the asset doesn't exist.
std::string FindEditorAsset(const std::string& file) {
    auto& vfs = Luma::VFS::VFS::Global();
    if (!vfs.IsMounted(Luma::VFS::Root::Content)) return {};
    Luma::VFS::Path p(Luma::VFS::Root::Content, "Editor/" + file);
    if (!vfs.Exists(p)) return {};
    auto real = vfs.TryResolve(p);
    if (!real) return {};
    return real->string();
}

// Feeds a window event into the Slate context (and reports window close).
void FeedEvent(Slate::Context& ui, Event& e, bool& running, EditorScreen* editor = nullptr) {
    EventDispatcher d(e);
    d.Dispatch<WindowCloseEvent>([&](WindowCloseEvent&) {
        running = false;
        return true;
    });
    d.Dispatch<WindowDropEvent>([&](WindowDropEvent& drop) {
        if (editor) {
            editor->HandleDroppedFiles(drop.Paths());
        }
        return true;
    });
    d.Dispatch<MouseMovedEvent>([&](MouseMovedEvent& m) {
        ui.OnMouseMove(m.X(), m.Y());
        return false;
    });
    d.Dispatch<MouseButtonPressedEvent>([&](MouseButtonPressedEvent& m) {
        ui.OnMouseButton(m.Button(), true);
        return false;
    });
    d.Dispatch<MouseButtonReleasedEvent>([&](MouseButtonReleasedEvent& m) {
        ui.OnMouseButton(m.Button(), false);
        return false;
    });
    d.Dispatch<MouseScrolledEvent>([&](MouseScrolledEvent& m) {
        ui.OnScroll(m.OffsetY());
        return false;
    });
    d.Dispatch<KeyTypedEvent>([&](KeyTypedEvent& k) {
        ui.OnText(k.Codepoint());
        return false;
    });
    d.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& k) {
        ui.OnKey(k.Keycode(), true);
        return false;
    });
}

}  // namespace

int main(int argc, char** argv) {
    Log::Init(LogLevel::Trace);
    Log::AddSink(Log::MakeConsoleSink());
    
    // Use absolute path for log file to avoid VFS remounting issues
    std::error_code logEc;
    std::filesystem::path logDir = std::filesystem::absolute("Saved/Logs", logEc);
    std::filesystem::create_directories(logDir, logEc);
    std::filesystem::path logFile = logDir / "Editor.log";
    Log::AddSink(Log::MakeFileSink(logFile.string()));
    
    // Test logging immediately
    LUMA_LOG_INFO("Editor", "Logging system initialized");
    LUMA_LOG_INFO("Editor", "Log file: {}", logFile.string());
    
    // Flush logs to ensure immediate write
    Luma::Log::Flush();

    // Parse mode: --project <path.luma> opens the editor; otherwise the browser.
    std::filesystem::path projectFile;
    std::string screenshotPath;
    bool editorMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--project" && i + 1 < argc) {
            projectFile = argv[++i];
            editorMode = true;
        } else if (arg == "--project-manager") {
            editorMode = false;
        } else if (arg == "--screenshot" && i + 1 < argc) {
            screenshotPath = argv[++i];
        }
    }

    // Touch the VFS and (in editor mode) remount Project/Saved/Intermediate
    // under the active project so project-relative content and logs resolve
    // through the same paths.
    {
        auto& vfs = Luma::VFS::VFS::Global();
        if (editorMode && !projectFile.empty()) {
            auto projectRoot = Luma::VFS::ProjectRootFromFile(projectFile);
            if (!projectRoot.empty()) {
                vfs.Mount(Luma::VFS::Root::Project, projectRoot);
                // Scratch data (logs, screenshots, caches) lives in the
                // project's Intermediate/ folder at the project root — both
                // the legacy "Saved" and "Intermediate" roots resolve there.
                vfs.Mount(Luma::VFS::Root::Saved,
                          projectRoot / "Intermediate");
                vfs.Mount(Luma::VFS::Root::Intermediate,
                          projectRoot / "Intermediate");
                LUMA_LOG_INFO("Editor", "mounted project at {}",
                              projectRoot.string());
            }
        }
    }

    WindowProps props;
    props.title = editorMode ? "Luma Editor" : "Luma - Project Browser";
    props.width = editorMode ? 1360u : 900u;
    props.height = editorMode ? 820u : 640u;
    std::unique_ptr<Window> window = Window::Create(props);

    RendererConfig rc;
    rc.appName = "Luma Editor";
    rc.vsync = true;
    std::unique_ptr<Renderer> renderer = CreateVulkanRenderer(*window, rc);

    // Initialize thumbnail system (after renderer is ready for GPU rendering)
    {
        auto& thumbnailMgr = Luma::ThumbnailManager::Get();
        thumbnailMgr.Initialize();
        
        // Register thumbnail renderers
        thumbnailMgr.RegisterRenderer(Luma::AssetType::Texture, 
                                     std::make_unique<Luma::TextureThumbnailRenderer>());
        
        // Initialize mesh thumbnail renderer with RHI device for potential GPU rendering
        auto meshRenderer = std::make_unique<Luma::MeshThumbnailRenderer>();
        meshRenderer->Initialize(nullptr);  // CPU software rendering; GPU paths available later
        thumbnailMgr.RegisterRenderer(Luma::AssetType::Mesh, std::move(meshRenderer));

        // Material thumbnails: a studio-lit sphere shaded with the .lmat's
        // constant fallbacks (base color / metallic / roughness / emissive).
        thumbnailMgr.RegisterRenderer(Luma::AssetType::Material,
                                     std::make_unique<Luma::MaterialThumbnailRenderer>());
        
        LUMA_LOG_INFO("Editor", "Thumbnail system initialized with {} renderers", 3);
    }

    // Fonts: Red Hat Display (variable font) is the primary UI family. The variable font
    // supports multiple weights through a single file. We'll use the same variable font
    // for all weights and let the renderer handle the weight variations.
    //   uiRegular -> body / default text
    //   uiMedium  -> captions / hint text / weaker emphasis
    //   uiSemiBold-> section headings, tab titles, button labels
    //   uiBold    -> titles, splash, large display
    //   mono      -> console output, logs, code
    // If Red Hat Display can't be located, fall back to Segoe UI on Windows.
    auto fontPathOr = [](const std::string& primary, const char* fallback) {
        return primary.empty() ? std::string(fallback) : primary;
    };

    // Use Red Hat Display variable font for all weights
    std::string redHatFont = "D:/Luma Engine/Red_Hat_Display/RedHatDisplay-VariableFont_wght.ttf";
    std::string bodyFont = fontPathOr(redHatFont, "C:/Windows/Fonts/segoeui.ttf");
    std::string mediumFont = fontPathOr(redHatFont, bodyFont.c_str());
    std::string uiFont = fontPathOr(redHatFont, bodyFont.c_str());
    std::string titleFont = fontPathOr(redHatFont, "C:/Windows/Fonts/segoeui.ttf");
    std::string monoFont =
        "C:/Windows/Fonts/consola.ttf";  // always present on Windows

    Slate::Context ui;  // owns the loaded fonts + theme tokens
    // Build the typography scale on the theme. Sizes are tuned for a 96 DPI
    // baseline; will scale crisply with the renderer's DPI factor.
    Slate::Typography& type = ui.theme().type;
    type.uiRegular = bodyFont;
    type.uiMedium = mediumFont;
    type.uiSemiBold = uiFont;
    type.uiBold = titleFont;
    type.mono = monoFont;
    // Further increased font sizes for maximum quality and readability
    type.captionSize = 15.0f;   // increased from 14.0f
    type.bodySize = 18.0f;      // increased from 16.0f
    type.bodyStrongSize = 18.0f; // increased from 16.0f
    type.headingSize = 18.0f;   // increased from 16.0f
    type.titleSize = 28.0f;     // increased from 26.0f
    type.displaySize = 38.0f;   // increased from 36.0f
    if (!ui.Init(*renderer, type, window->ContentScale())) {
        LUMA_LOG_ERROR("Editor", "failed to load UI font");
    }
    // Window icon from the engine icon asset.
    {
        std::string iconPath = FindEditorAsset("luma_icon.png");
        u32 iw = 0, ih = 0;
        std::vector<u8> iconPixels;
        if (!iconPath.empty() &&
            Slate::LoadImagePixels(iconPath, iw, ih, iconPixels)) {
            window->SetIcon(iw, ih, iconPixels.data());
        }
    }
    // Logo for the banner / splash.
    Slate::Image logo = {};
    {
        std::string logoPath = FindEditorAsset("luma_logo.png");
        if (!logoPath.empty()) logo = Slate::LoadImage(*renderer, logoPath);
    }

    bool running = true;
    std::unique_ptr<EditorScreen> editor;
    window->SetEventCallback(
        [&](Event& e) { FeedEvent(ui, e, running, editor.get()); });

    // Template thumbnails.
    ProjectBrowser browser;
    browser.SetLogo(logo);
    struct TemplateAsset {
        GameTemplate value;
        const char* file;
    };
    const TemplateAsset kTemplateAssets[] = {
        {GameTemplate::Empty, "Templates/Empty.png"},
        {GameTemplate::FirstPerson, "Templates/FirstPerson.png"},
        {GameTemplate::ThirdPerson, "Templates/ThirdPerson.png"},
        {GameTemplate::TopDown, "Templates/TopDown.png"},
    };
    for (const TemplateAsset& asset : kTemplateAssets) {
        std::string path = FindEditorAsset(asset.file);
        if (!path.empty()) {
            browser.SetTemplateThumbnail(asset.value,
                                         Slate::LoadImage(*renderer, path));
        }
    }

    // Editor toolbar icons.
    auto loadIcon = [&](const char* file) -> TextureHandle {
        std::string path = FindEditorAsset(file);
        return path.empty() ? 0 : Slate::LoadImage(*renderer, path).texture;
    };
    TextureHandle iconPlay = loadIcon("Icons/play.png");
    TextureHandle iconPause = loadIcon("Icons/pause.png");
    TextureHandle iconStop = loadIcon("Icons/stop.png");
    TextureHandle iconLogo = loadIcon("luma_icon.png");
    // Content Browser chrome icons (sort arrows + search glass + folder +
    // reload + import + open-folder).
    TextureHandle cbSortUp = loadIcon("Icons/sort_up.png");
    TextureHandle cbSortDown = loadIcon("Icons/sort_down.png");
    TextureHandle cbSearchGlass = loadIcon("Icons/search_glass.png");
    TextureHandle cbFolder = loadIcon("Icons/folder_base.png");
    TextureHandle cbReload = loadIcon("Icons/reload.png");
    TextureHandle cbImport = loadIcon("Icons/import.png");
    TextureHandle cbOpenFolder = loadIcon("Icons/open-folder.png");
    TextureHandle cbExpandArrow = loadIcon("Icons/SubmenuArrow.png");
    TextureHandle cbRetractArrow = loadIcon("Icons/SortDownArrow.png");

    // World Outliner icons (Create-menu + actor-row textures).
    TextureHandle olCreateButton = loadIcon("Icons/Create-button.png");
    TextureHandle olSearchGlass = loadIcon("Icons/SearchGlass.png");
    TextureHandle olCatGeometry = loadIcon("Icons/geometry.png");
    TextureHandle olCatLight = loadIcon("Icons/light.png");
    TextureHandle olCatEnvironment = loadIcon("Icons/environment.png");
    TextureHandle olActorCube = loadIcon("Icons/actor-cube.png");
    TextureHandle olActorPlane = loadIcon("Icons/actor-plane.png");
    TextureHandle olActorSphere = loadIcon("Icons/actor-sphere.png");
    TextureHandle olActorCylinder = loadIcon("Icons/actor-cylinder.png");
    TextureHandle olActorDirLight =
        loadIcon("Icons/actor-light-directional.png");
    TextureHandle olActorPointLight =
        loadIcon("Icons/actor-light-point.png");
    TextureHandle olActorSpotLight = loadIcon("Icons/actor-light-spot.png");
    TextureHandle olActorMesh = loadIcon("Icons/actor-mesh.png");

    SplashScreen splash;

    // Initialize Vulkan deferred renderer (editor mode only)
    void* deferredRendererPtr = renderer->GetVulkanDeferredRenderer();
    if (deferredRendererPtr) {
        LUMA_LOG_INFO("Editor", "Vulkan deferred renderer available");
    } else {
        LUMA_LOG_WARN("Editor", "Vulkan deferred renderer not available");
    }
    Log::Flush();
    if (editorMode) {
        editor = std::make_unique<EditorScreen>(projectFile);
        editor->SetRenderer(renderer.get());
        editor->SetToolbarIcons(iconPlay, iconPause, iconStop);
        editor->SetLogoIcon(iconLogo);
        editor->SetContentBrowserIcons(cbSortUp, cbSortDown, cbSearchGlass,
                                       cbFolder, cbReload, cbImport,
                                       cbOpenFolder, cbExpandArrow,
                                       cbRetractArrow);
        // World Outliner icons: Create-menu header button + search glass +
        // category rows + primitive rows + actor-row icons beside entity
        // names. Zero handles fall back to procedural glyphs.
        editor->SetCreateButtonIcon(olCreateButton);
        editor->SetSearchGlassIcon(olSearchGlass);
        editor->SetCategoryIcons(olCatGeometry, olCatLight, olCatEnvironment);
        editor->SetPrimitiveIcons(olActorCube, olActorPlane, olActorSphere,
                                  olActorCylinder);
        editor->SetOutlinerActorIcons(olActorDirLight, olActorPointLight,
                                      olActorSpotLight, olActorMesh);
    }

    // Splash phase (editor mode only).
    constexpr f32 kSplashDuration = 1.6f;
    f32 splashTime = 0.0f;
    // For a screenshot, skip the splash and capture after the UI settles.
    if (!screenshotPath.empty()) splashTime = kSplashDuration;
    int frameCount = 0;
    const int kCaptureFrame = 30;

    LUMA_LOG_INFO("Editor", "starting in {} mode",
                  editorMode ? "editor" : "project-browser");

    FrameClock clock;
    while (running && !window->ShouldClose()) {
        window->PollEvents();
        Timestep dt = clock.Tick();

        // Render the 3D scene into the editor viewport (using last frame's rect),
        // before opening the swapchain frame.
        if (editorMode && editor && splashTime >= kSplashDuration) {
            Slate::Rect vp = editor->ViewportRect();
            if (vp.w > 4.0f && vp.h > 4.0f) {
                // Use ONLY Vulkan deferred renderer for viewport
                if (deferredRendererPtr) {
                    auto* vulkanDeferredRenderer = static_cast<Rendering::VulkanDeferredRenderer*>(deferredRendererPtr);
                    
                    // Build scene view for Vulkan deferred renderer
                    Renderer2::DeferredSceneView deferredScene = editor->BuildDeferredSceneView();
                    deferredScene.width = static_cast<u32>(vp.w);
                    deferredScene.height = static_cast<u32>(vp.h);
                    deferredScene.projectionMatrix = Math::Perspective(
                        deferredScene.fov,
                        static_cast<f32>(vp.w) / static_cast<f32>(vp.h),
                        deferredScene.nearPlane, deferredScene.farPlane);
                    deferredScene.viewProjectionMatrix =
                        deferredScene.projectionMatrix * deferredScene.viewMatrix;
                    
                    // Set viewport dimensions and render
                    vulkanDeferredRenderer->SetViewportDimensions(static_cast<u32>(vp.w), static_cast<u32>(vp.h));
                    vulkanDeferredRenderer->PrepareScene();
                    vulkanDeferredRenderer->RenderScene(deferredScene);
                    
                    // The deferred renderer owns the UI texture handle for its
                    // light-accumulation buffer (re-pointed internally on resize).
                    TextureHandle sceneTex = vulkanDeferredRenderer->GetViewportTextureHandle();
                    if (sceneTex != 0) {
                        editor->SetViewportTexture(sceneTex);
                    }
                    
                    // Log viewport render (only once per second to avoid spam)
                    static u32 lastLogFrame = 0;
                    if (frameCount - lastLogFrame > 60) {
                        LUMA_LOG_DEBUG("Editor", "Viewport rendered {}x{} with Vulkan deferred renderer", 
                                     static_cast<u32>(vp.w), static_cast<u32>(vp.h));
                        lastLogFrame = frameCount;
                        Log::Flush();
                    }
                } else {
                    LUMA_LOG_ERROR("Editor", "Vulkan deferred renderer not available - viewport will be black");
                }
            }
        }

        if (!renderer->BeginFrame()) continue;
        ui.BeginFrame(static_cast<f32>(window->Width()),
                      static_cast<f32>(window->Height()), dt);

        f32 w = static_cast<f32>(window->Width());
        f32 h = static_cast<f32>(window->Height());

        if (editorMode) {
            if (splashTime < kSplashDuration) {
                splashTime += dt;
                f32 progress = splashTime / kSplashDuration;
                const char* msg = "Starting editor...";
                if (progress < 0.3f) msg = "Initializing Vulkan...";
                else if (progress < 0.6f) msg = "Loading project...";
                else if (progress < 0.85f) msg = "Compiling shaders...";
                splash.Draw(ui, w, h, progress, msg, logo);
            } else {
                editor->Draw(ui, w, h);
            }
        } else {
            BrowserResult result = browser.Draw(ui, w, h);
            if (result.launch) {
                // Godot-style: relaunch this exe in editor mode, then exit.
                LUMA_LOG_INFO("Editor", "opening project: {}",
                              result.projectFile.string());
                LaunchDetached(ExecutablePath(),
                               {"--project", result.projectFile.string()});
                running = false;
            }
        }

        renderer->DrawUI(ui.EndFrame());
        window->SetCursor(ui.RequestedCursor());

        if (!screenshotPath.empty() && frameCount == kCaptureFrame) {
            renderer->CaptureFrame(screenshotPath);
        }
        renderer->EndFrame();
        
        // Flush logs periodically to ensure they're written to disk
        Luma::Log::Flush();

        if (!screenshotPath.empty() && frameCount >= kCaptureFrame) {
            running = false;  // captured; exit
        }
        ++frameCount;
    }

    renderer->WaitIdle();
    editor.reset();
    renderer.reset();
    window.reset();
    
    // Flush logs before shutdown
    Log::Flush();
    
    Log::Shutdown();
    return 0;
}
