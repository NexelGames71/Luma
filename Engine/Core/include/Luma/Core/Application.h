#pragma once

#include <memory>
#include <string>

#include "Luma/Core/Event.h"
#include "Luma/Core/Events.h"
#include "Luma/Core/Layer.h"
#include "Luma/Core/LayerStack.h"
#include "Luma/Core/Timestep.h"
#include "Luma/Core/Types.h"

// The application core: owns the layer stack, routes events, and advances one
// frame at a time. It is deliberately window-agnostic so it can be unit-tested
// headlessly; the platform window and the real timing loop are wired up by the
// runtime (Runtime/Sandbox) which owns a Window and calls RunOneFrame.

namespace Luma {

struct ApplicationSpec {
    std::string name = "Luma Application";
    u32 width = 1280;
    u32 height = 720;
};

class Application {
public:
    explicit Application(ApplicationSpec spec);
    virtual ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    Layer* PushLayer(std::unique_ptr<Layer> layer);
    Layer* PushOverlay(std::unique_ptr<Layer> overlay);

    // Feeds an event to the app (window close closes the app) and then to the
    // layers top-to-bottom until one marks it Handled.
    void OnEvent(Event& event);

    // Advances every layer by dt (bottom-to-top).
    void RunOneFrame(Timestep dt);

    void Close() { m_running = false; }
    bool IsRunning() const { return m_running; }

    const ApplicationSpec& Spec() const { return m_spec; }
    LayerStack& Layers() { return m_layerStack; }

    static Application* Get() { return s_instance; }

private:
    bool OnWindowClose(WindowCloseEvent& event);

    ApplicationSpec m_spec;
    LayerStack m_layerStack;
    bool m_running = true;

    static Application* s_instance;
};

}  // namespace Luma
