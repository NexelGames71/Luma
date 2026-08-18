#include "Luma/RenderGraph/RenderGraphBuilder.h"

namespace Luma {
namespace RenderGraph {

RenderGraphBuilder::RenderGraphBuilder(RenderGraph* graph)
    : m_graph(graph) {
}

RenderGraphBuilder::~RenderGraphBuilder() {
}

RenderGraphBuilder& RenderGraphBuilder::AddPass(RenderPass* pass) {
    if (m_graph && pass) {
        m_graph->AddPass(pass);
    }
    return *this;
}

RenderGraphBuilder& RenderGraphBuilder::SetDimensions(u32 width, u32 height) {
    if (m_graph) {
        m_graph->SetDimensions(width, height);
    }
    return *this;
}

bool RenderGraphBuilder::Build() {
    if (!m_graph) {
        return false;
    }
    
    m_built = m_graph->Setup();
    return m_built;
}

} // namespace RenderGraph
} // namespace Luma