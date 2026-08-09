#pragma once

#include <memory>
#include <vector>

#include "Luma/Core/Layer.h"
#include "Luma/Core/Types.h"

// Owns layers and overlays. Regular layers occupy the front portion of the
// vector; overlays are always appended after them, so overlays stay on top even
// as more layers are pushed. Forward iteration = bottom-to-top (update order);
// reverse iteration = top-to-bottom (event order).

namespace Luma {

class LayerStack {
public:
    using Container = std::vector<std::unique_ptr<Layer>>;

    LayerStack() = default;
    ~LayerStack();

    LayerStack(const LayerStack&) = delete;
    LayerStack& operator=(const LayerStack&) = delete;

    // Both take ownership and call OnAttach immediately. Returns a borrowed
    // (non-owning) pointer for convenience.
    Layer* PushLayer(std::unique_ptr<Layer> layer);
    Layer* PushOverlay(std::unique_ptr<Layer> overlay);

    // Removes and detaches a previously-pushed layer/overlay. No-op if absent.
    void PopLayer(Layer* layer);
    void PopOverlay(Layer* overlay);

    void Clear();

    usize Size() const { return m_layers.size(); }

    Container::iterator begin() { return m_layers.begin(); }
    Container::iterator end() { return m_layers.end(); }
    Container::reverse_iterator rbegin() { return m_layers.rbegin(); }
    Container::reverse_iterator rend() { return m_layers.rend(); }

    Container::const_iterator begin() const { return m_layers.begin(); }
    Container::const_iterator end() const { return m_layers.end(); }

private:
    Container m_layers;
    // Number of non-overlay layers; also the insertion point for the next layer.
    usize m_layerCount = 0;
};

}  // namespace Luma
