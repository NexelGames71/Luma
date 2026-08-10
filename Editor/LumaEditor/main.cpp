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
#include "Luma/Slate/Context.h"
#include "Luma/Slate/Image.h"

#include "EditorScreen.h"
#include "SplashScreen.h"

using namespace Luma;

namespace {

// Finds an editor asset (e.g. logo) by searching Content/Editor upward from the
// working directory.
std::string FindEditorAsset(const std::string& file) {
    const char* prefixes[] = {"Content/Editor/", "../Content/Editor/",
                              "../../Content/Editor/",
                              "../../../Content/Editor/"};
    for (const char* p : prefixes) {
        std::string candidate = std::string(p) + file;
        if (std::filesystem::exists(candidate)) return candidate;
    }
    return {};
}

// Feeds a window event into the Slate context (and reports window close).
void FeedEvent(Slate::Context& ui, Event& e, bool& running) {
    EventDispatcher d(e);
    d.Dispatch<WindowCloseEvent>([&](WindowCloseEvent&) {
        running = false;
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
    std::error_code logEc;
    std::filesystem::create_directories("Saved/Logs", logEc);
    Log::AddSink(Log::MakeFileSink("Saved/Logs/Editor.log"));

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

    WindowProps props;
    props.title = editorMode ? "Luma Editor" : "Luma - Project Browser";
    props.width = editorMode ? 1360u : 900u;
    props.height = editorMode ? 820u : 640u;
    std::unique_ptr<Window> window = Window::Create(props);

    RendererConfig rc;
    rc.appName = "Luma Editor";
    rc.vsync = true;
    std::unique_ptr<Renderer> renderer = CreateVulkanRenderer(*window, rc);

    // Fonts: Open Sans (bundled), falling back to Segoe UI if not found.
    std::string bodyFont = FindEditorAsset("Fonts/OpenSans-Regular.ttf");
    std::string titleFont = FindEditorAsset("Fonts/OpenSans-SemiBold.ttf");
    if (bodyFont.empty()) bodyFont = "C:/Windows/Fonts/segoeui.ttf";
    if (titleFont.empty()) titleFont = bodyFont;
    Slate::Context ui;
    if (!ui.Init(*renderer, bodyFont, titleFont)) {
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
    window->SetEventCallback(
        [&](Event& e) { FeedEvent(ui, e, running); });

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

    std::unique_ptr<EditorScreen> editor;
    SplashScreen splash;
    if (editorMode) {
        editor = std::make_unique<EditorScreen>(projectFile);
        editor->SetToolbarIcons(iconPlay, iconPause, iconStop);
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
                TextureHandle sceneTex = renderer->RenderSceneView(
                    static_cast<u32>(vp.w), static_cast<u32>(vp.h), dt);
                editor->SetViewportTexture(sceneTex);
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

        if (!screenshotPath.empty() && frameCount == kCaptureFrame) {
            renderer->CaptureFrame(screenshotPath);
        }
        renderer->EndFrame();

        if (!screenshotPath.empty() && frameCount >= kCaptureFrame) {
            running = false;  // captured; exit
        }
        ++frameCount;
    }

    renderer->WaitIdle();
    editor.reset();
    renderer.reset();
    window.reset();
    Log::Shutdown();
    return 0;
}
