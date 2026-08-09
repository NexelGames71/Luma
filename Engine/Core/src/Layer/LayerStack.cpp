#include "Luma/Core/LayerStack.h"

#include <algorithm>

namespace Luma {

LayerStack::~LayerStack() { Clear(); }

Layer* LayerStack::PushLayer(std::unique_ptr<Layer> layer) {
    Layer* raw = layer.get();
    m_layers.insert(m_layers.begin() + static_cast<isize>(m_layerCount),
                    std::move(layer));
    ++m_layerCount;
    raw->OnAttach();
    return raw;
}

Layer* LayerStack::PushOverlay(std::unique_ptr<Layer> overlay) {
    Layer* raw = overlay.get();
    m_layers.push_back(std::move(overlay));
    raw->OnAttach();
    return raw;
}

void LayerStack::PopLayer(Layer* layer) {
    auto it = std::find_if(
        m_layers.begin(), m_layers.begin() + static_cast<isize>(m_layerCount),
        [layer](const std::unique_ptr<Layer>& l) { return l.get() == layer; });
    if (it != m_layers.begin() + static_cast<isize>(m_layerCount)) {
        (*it)->OnDetach();
        m_layers.erase(it);
        --m_layerCount;
    }
}

void LayerStack::PopOverlay(Layer* overlay) {
    auto it = std::find_if(
        m_layers.begin() + static_cast<isize>(m_layerCount), m_layers.end(),
        [overlay](const std::unique_ptr<Layer>& l) {
            return l.get() == overlay;
        });
    if (it != m_layers.end()) {
        (*it)->OnDetach();
        m_layers.erase(it);
    }
}

void LayerStack::Clear() {
    // Detach in reverse (top-to-bottom) before destroying.
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
        (*it)->OnDetach();
    }
    m_layers.clear();
    m_layerCount = 0;
}

}  // namespace Luma
