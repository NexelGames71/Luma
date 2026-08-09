#pragma once

#include <string>

#include "Luma/Core/Event.h"
#include "Luma/Core/Types.h"

// A Layer is a slice of the application (engine, editor, game, debug UI, ...).
// The LayerStack updates layers bottom-to-top each frame and delivers events
// top-to-bottom. Overlays always sit above regular layers.

namespace Luma {

class Layer {
public:
    explicit Layer(std::string name = "Layer") : m_name(std::move(name)) {}
    virtual ~Layer() = default;

    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void OnUpdate(f32 deltaSeconds) { LUMA_UNUSED(deltaSeconds); }
    virtual void OnEvent(Event& event) { LUMA_UNUSED(event); }

    const std::string& Name() const { return m_name; }

protected:
    std::string m_name;
};

}  // namespace Luma
