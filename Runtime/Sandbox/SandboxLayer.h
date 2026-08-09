#pragma once

#include "Luma/Core/Application.h"
#include "Luma/Core/Event.h"
#include "Luma/Core/Events.h"
#include "Luma/Core/Layer.h"
#include "Luma/Core/Log.h"
#include "Luma/Core/Types.h"

namespace Luma {

// Minimal demonstration layer for Milestone 1: logs lifecycle + input, and lets
// Escape close the app. Real gameplay/editor layers replace this later.
class SandboxLayer final : public Layer {
public:
    SandboxLayer() : Layer("Sandbox") {}

    void OnAttach() override { LUMA_LOG_INFO("Sandbox", "layer attached"); }
    void OnDetach() override { LUMA_LOG_INFO("Sandbox", "layer detached"); }

    void OnUpdate(f32 /*dt*/) override {
        // No per-frame work yet; rendering arrives in Milestone 2.
    }

    void OnEvent(Event& event) override {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>([](KeyPressedEvent& key) {
            LUMA_LOG_DEBUG("Sandbox", "key {} pressed{}", key.Keycode(),
                           key.IsRepeat() ? " (repeat)" : "");
            if (key.Keycode() == kKeyEscape) {
                if (auto* app = Application::Get()) app->Close();
            }
            return false;  // don't consume; let other layers see it too
        });
    }

private:
    // GLFW_KEY_ESCAPE. Kept as a literal so Core stays free of GLFW headers;
    // a proper Luma keycode enum arrives with the input system.
    static constexpr i32 kKeyEscape = 256;
};

}  // namespace Luma
