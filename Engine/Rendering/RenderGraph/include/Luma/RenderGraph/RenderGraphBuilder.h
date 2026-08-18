#pragma once

#include "Luma/RenderGraph/RenderGraph.h"

// Render graph builder for constructing render graphs programmatically.
// Provides a fluent interface for building complex render pipelines.

namespace Luma {
namespace RenderGraph {

// Render graph builder interface
class RenderGraphBuilder {
public:
    RenderGraphBuilder(RenderGraph* graph);
    ~RenderGraphBuilder();
    
    // Add a render pass
    RenderGraphBuilder& AddPass(RenderPass* pass);
    
    // Set graph dimensions
    RenderGraphBuilder& SetDimensions(u32 width, u32 height);
    
    // Build the graph
    bool Build();
    
    // Get the built graph
    RenderGraph* GetGraph() const { return m_graph; }
    
private:
    RenderGraph* m_graph;
    bool m_built = false;
};

// Convenience function to create a builder
inline RenderGraphBuilder* CreateBuilder(RenderGraph* graph) {
    return new RenderGraphBuilder(graph);
}

} // namespace RenderGraph
} // namespace Luma