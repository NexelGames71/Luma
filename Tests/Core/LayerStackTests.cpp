#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "Luma/Core/LayerStack.h"

namespace {

// Records attach/detach into a shared log so ordering can be asserted.
class ProbeLayer final : public Luma::Layer {
public:
    ProbeLayer(std::string name, std::vector<std::string>* log)
        : Luma::Layer(std::move(name)), m_log(log) {}
    void OnAttach() override { m_log->push_back("attach:" + Name()); }
    void OnDetach() override { m_log->push_back("detach:" + Name()); }

private:
    std::vector<std::string>* m_log;
};

std::vector<std::string> Names(const Luma::LayerStack& stack) {
    std::vector<std::string> names;
    for (const auto& layer : stack) names.push_back(layer->Name());
    return names;
}

}  // namespace

TEST_CASE("Overlays stay above layers regardless of push order",
          "[core][layerstack]") {
    std::vector<std::string> log;
    Luma::LayerStack stack;

    stack.PushLayer(std::make_unique<ProbeLayer>("L0", &log));
    stack.PushOverlay(std::make_unique<ProbeLayer>("Overlay", &log));
    stack.PushLayer(std::make_unique<ProbeLayer>("L1", &log));

    // L1 must be inserted before the overlay.
    REQUIRE(Names(stack) == std::vector<std::string>{"L0", "L1", "Overlay"});
    REQUIRE(stack.Size() == 3);
}

TEST_CASE("Push calls OnAttach immediately", "[core][layerstack]") {
    std::vector<std::string> log;
    Luma::LayerStack stack;
    stack.PushLayer(std::make_unique<ProbeLayer>("A", &log));
    stack.PushOverlay(std::make_unique<ProbeLayer>("Ov", &log));
    REQUIRE(log == std::vector<std::string>{"attach:A", "attach:Ov"});
}

TEST_CASE("Clear detaches top-to-bottom", "[core][layerstack]") {
    std::vector<std::string> log;
    {
        Luma::LayerStack stack;
        stack.PushLayer(std::make_unique<ProbeLayer>("A", &log));
        stack.PushLayer(std::make_unique<ProbeLayer>("B", &log));
        stack.PushOverlay(std::make_unique<ProbeLayer>("Ov", &log));
        log.clear();
    }  // stack destructor -> Clear()
    REQUIRE(log == std::vector<std::string>{"detach:Ov", "detach:B", "detach:A"});
}

TEST_CASE("PopLayer detaches and removes only the target",
          "[core][layerstack]") {
    std::vector<std::string> log;
    Luma::LayerStack stack;
    Luma::Layer* a = stack.PushLayer(std::make_unique<ProbeLayer>("A", &log));
    stack.PushLayer(std::make_unique<ProbeLayer>("B", &log));
    log.clear();

    stack.PopLayer(a);
    REQUIRE(log == std::vector<std::string>{"detach:A"});
    REQUIRE(Names(stack) == std::vector<std::string>{"B"});
}
