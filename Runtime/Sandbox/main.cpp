#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>

#include "Luma/Core/Application.h"
#include "Luma/Core/Config.h"
#include "Luma/Core/EngineLoop.h"
#include "Luma/Core/Events.h"
#include "Luma/Core/Log.h"
#include "Luma/Core/Types.h"
#include "Luma/Core/Version.h"
#include "Luma/Platform/Window.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/RHI/VulkanRenderer.h"

#include "SandboxLayer.h"

using namespace Luma;

namespace {

// Best-effort config load; searches upward so it works whether launched from the
// repo root or from build/bin/<Config>/.
Config LoadEngineConfig() {
    Config cfg;
    const char* candidates[] = {
        "Config/Engine.ini", "../Config/Engine.ini", "../../Config/Engine.ini",
        "../../../Config/Engine.ini"};
    for (const char* path : candidates) {
        if (cfg.LoadFromFile(path)) {
            LUMA_LOG_INFO("Boot", "loaded config: {}", path);
            return cfg;
        }
    }
    LUMA_LOG_WARN("Boot", "no Engine.ini found; using built-in defaults");
    return cfg;
}

}  // namespace

int main() {
    Log::Init(LogLevel::Trace);
    std::error_code ec;
    std::filesystem::create_directories("Saved/Logs", ec);
    Log::AddSink(Log::MakeFileSink("Saved/Logs/Luma.log"));

    LUMA_LOG_INFO("Boot", "starting {}", EngineVersionString());

    Config cfg = LoadEngineConfig();

    ApplicationSpec spec;
    spec.name = cfg.GetString("Application.name", "Luma Engine");
    spec.width = static_cast<u32>(cfg.GetInt("Window.width", 1280));
    spec.height = static_cast<u32>(cfg.GetInt("Window.height", 720));

    Application app(spec);

    WindowProps props;
    props.title = cfg.GetString("Window.title", spec.name);
    props.width = spec.width;
    props.height = spec.height;

    std::unique_ptr<Window> window = Window::Create(props);

    RendererConfig rendererConfig;
    rendererConfig.appName = spec.name;
    rendererConfig.enableValidation = true;
    rendererConfig.vsync = cfg.GetBool("Window.vsync", true);
    std::unique_ptr<Renderer> renderer =
        CreateVulkanRenderer(*window, rendererConfig);
    renderer->SetClearColor(ClearColor{0.10f, 0.12f, 0.16f, 1.0f});

    // Route events to the app, and feed resizes to the renderer.
    window->SetEventCallback([&app, &renderer](Event& e) {
        app.OnEvent(e);
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowResizeEvent>([&renderer](WindowResizeEvent& r) {
            renderer->OnResize(r.Width(), r.Height());
            return false;
        });
    });

    app.PushLayer(std::make_unique<SandboxLayer>());

    LUMA_LOG_INFO("Boot", "entering main loop");
    FrameClock clock;
    f32 fpsAccum = 0.0f;
    int frames = 0;
    while (app.IsRunning() && !window->ShouldClose()) {
        window->PollEvents();

        Timestep dt = clock.Tick();
        app.RunOneFrame(dt);

        if (renderer->BeginFrame()) {
            // Milestone 2: clear only. Geometry/UI recording lands in M3+.
            renderer->EndFrame();
        }

        fpsAccum += dt.Seconds();
        ++frames;
        if (fpsAccum >= 1.0f) {
            LUMA_LOG_TRACE("Perf", "{} fps ({:.2f} ms/frame)", frames,
                           (fpsAccum / static_cast<f32>(frames)) * 1000.0f);
            fpsAccum = 0.0f;
            frames = 0;
        }

        // Without vsync/rendering, yield briefly so we don't spin a core at 100%.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    LUMA_LOG_INFO("Boot", "main loop exited; shutting down");
    renderer->WaitIdle();
    renderer.reset();
    window.reset();
    Log::Shutdown();
    return 0;
}
