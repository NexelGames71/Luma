#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "Luma/Core/Application.h"
#include "Luma/Core/EngineLoop.h"
#include "Luma/Core/Events.h"

namespace {

class ProbeLayer final : public Luma::Layer {
public:
    ProbeLayer(std::string name, bool consumeEvents)
        : Luma::Layer(std::move(name)), m_consume(consumeEvents) {}

    void OnUpdate(Luma::f32 dt) override {
        ++updateCount;
        lastDelta = dt;
    }
    void OnEvent(Luma::Event& e) override {
        ++eventCount;
        if (m_consume) e.Handled = true;
    }

    int updateCount = 0;
    int eventCount = 0;
    Luma::f32 lastDelta = 0.0f;

private:
    bool m_consume;
};

}  // namespace

TEST_CASE("RunOneFrame updates every layer with the delta",
          "[core][application]") {
    Luma::Application app(Luma::ApplicationSpec{"Test", 640, 480});
    auto* probe =
        static_cast<ProbeLayer*>(app.PushLayer(std::make_unique<ProbeLayer>("L", false)));

    app.RunOneFrame(Luma::Timestep(0.016f));
    app.RunOneFrame(Luma::Timestep(0.016f));

    REQUIRE(probe->updateCount == 2);
    REQUIRE(probe->lastDelta == 0.016f);
}

TEST_CASE("WindowCloseEvent stops the application", "[core][application]") {
    Luma::Application app(Luma::ApplicationSpec{"Test", 640, 480});
    REQUIRE(app.IsRunning());

    Luma::WindowCloseEvent close;
    app.OnEvent(close);

    REQUIRE_FALSE(app.IsRunning());
}

TEST_CASE("A consuming overlay stops event propagation to lower layers",
          "[core][application]") {
    Luma::Application app(Luma::ApplicationSpec{"Test", 640, 480});
    auto* base =
        static_cast<ProbeLayer*>(app.PushLayer(std::make_unique<ProbeLayer>("Base", false)));
    auto* overlay = static_cast<ProbeLayer*>(
        app.PushOverlay(std::make_unique<ProbeLayer>("Overlay", true)));

    Luma::KeyPressedEvent key(65, false);
    app.OnEvent(key);

    REQUIRE(overlay->eventCount == 1);
    REQUIRE(base->eventCount == 0);  // overlay consumed it
    REQUIRE(key.Handled);
}

TEST_CASE("FrameClock produces a non-negative bounded delta",
          "[core][engineloop]") {
    Luma::FrameClock clock;
    Luma::Timestep dt = clock.Tick();
    REQUIRE(dt.Seconds() >= 0.0f);
    REQUIRE(dt.Seconds() <= 0.25f);
}
