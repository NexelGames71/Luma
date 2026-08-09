#include "Luma/Core/Application.h"

#include "Luma/Core/Assert.h"
#include "Luma/Core/Log.h"

namespace Luma {

Application* Application::s_instance = nullptr;

Application::Application(ApplicationSpec spec) : m_spec(std::move(spec)) {
    LUMA_ASSERT(s_instance == nullptr, "Only one Application may exist");
    s_instance = this;
    LUMA_LOG_INFO("Application", "'{}' created ({}x{})", m_spec.name,
                  m_spec.width, m_spec.height);
}

Application::~Application() {
    m_layerStack.Clear();
    s_instance = nullptr;
}

Layer* Application::PushLayer(std::unique_ptr<Layer> layer) {
    return m_layerStack.PushLayer(std::move(layer));
}

Layer* Application::PushOverlay(std::unique_ptr<Layer> overlay) {
    return m_layerStack.PushOverlay(std::move(overlay));
}

void Application::OnEvent(Event& event) {
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<WindowCloseEvent>(
        [this](WindowCloseEvent& e) { return OnWindowClose(e); });

    // Deliver top-to-bottom; stop once a layer consumes it.
    for (auto it = m_layerStack.rbegin(); it != m_layerStack.rend(); ++it) {
        if (event.Handled) break;
        (*it)->OnEvent(event);
    }
}

void Application::RunOneFrame(Timestep dt) {
    for (auto& layer : m_layerStack) {
        layer->OnUpdate(dt);
    }
}

bool Application::OnWindowClose(WindowCloseEvent&) {
    m_running = false;
    LUMA_LOG_INFO("Application", "window close requested; shutting down");
    return true;
}

}  // namespace Luma
