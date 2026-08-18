#include "Luma/RenderGraph/RenderGraph.h"
#include "Luma/RenderGraph/RenderGraphResources.h"

#include <algorithm>
#include <string>

namespace Luma {
namespace RenderGraph {

using std::string;

// ============================================================================
// Render Graph Resources
// ============================================================================

RenderGraphResources::RenderGraphResources() {
}

RenderGraphResources::~RenderGraphResources() {
    Clear();
}

void RenderGraphResources::RegisterTexture(const string& name, RHI::RHITexture* texture, RHI::EResourceState initialState) {
    m_textures[name] = texture;
    m_resourceStates[name].state = initialState;
    m_resourceStates[name].lastUsedFrame = m_currentFrame;
    m_resourceStates[name].isTransient = false;
}

void RenderGraphResources::RegisterBuffer(const string& name, RHI::RHIBuffer* buffer, RHI::EResourceState initialState) {
    m_buffers[name] = buffer;
    m_resourceStates[name].state = initialState;
    m_resourceStates[name].lastUsedFrame = m_currentFrame;
    m_resourceStates[name].isTransient = false;
}

RHI::RHITexture* RenderGraphResources::GetTexture(const string& name) const {
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        return it->second;
    }
    return nullptr;
}

RHI::RHIBuffer* RenderGraphResources::GetBuffer(const string& name) const {
    auto it = m_buffers.find(name);
    if (it != m_buffers.end()) {
        return it->second;
    }
    return nullptr;
}

const ResourceState* RenderGraphResources::GetResourceState(const string& name) const {
    auto it = m_resourceStates.find(name);
    if (it != m_resourceStates.end()) {
        return &it->second;
    }
    return nullptr;
}

void RenderGraphResources::SetResourceState(const string& name, RHI::EResourceState state) {
    auto it = m_resourceStates.find(name);
    if (it != m_resourceStates.end()) {
        it->second.state = state;
        it->second.lastUsedFrame = m_currentFrame;
    }
}

bool RenderGraphResources::HasTexture(const string& name) const {
    return m_textures.find(name) != m_textures.end();
}

bool RenderGraphResources::HasBuffer(const string& name) const {
    return m_buffers.find(name) != m_buffers.end();
}

void RenderGraphResources::Clear() {
    m_textures.clear();
    m_buffers.clear();
    m_resourceStates.clear();
}

void RenderGraphResources::UpdateFrame(u32 frame) {
    m_currentFrame = frame;
    
    // Clean up old transient resources
    for (auto it = m_resourceStates.begin(); it != m_resourceStates.end(); ) {
        if (it->second.isTransient && (frame - it->second.lastUsedFrame) > 2) {
            // Remove old transient resource
            if (m_textures.count(it->first)) {
                m_textures.erase(it->first);
            }
            if (m_buffers.count(it->first)) {
                m_buffers.erase(it->first);
            }
            it = m_resourceStates.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Render Graph
// ============================================================================

RenderGraph::RenderGraph(const string& name)
    : m_name(name) {
    m_resources = new RenderGraphResources();
}

RenderGraph::~RenderGraph() {
    for (auto* pass : m_passes) {
        delete pass;
    }
    m_passes.clear();
    m_passMap.clear();
    
    if (m_resources) {
        delete m_resources;
    }
}

void RenderGraph::AddPass(RenderPass* pass) {
    if (!pass) {
        return;
    }
    
    m_passes.push_back(pass);
    m_passMap[pass->GetName()] = pass;
    m_valid = false;  // Need to rebuild
}

void RenderGraph::RemovePass(const string& name) {
    auto it = m_passMap.find(name);
    if (it != m_passMap.end()) {
        auto passIt = std::find(m_passes.begin(), m_passes.end(), it->second);
        if (passIt != m_passes.end()) {
            delete *passIt;
            m_passes.erase(passIt);
        }
        m_passMap.erase(it);
        m_valid = false;  // Need to rebuild
    }
}

RenderPass* RenderGraph::GetPass(const string& name) const {
    auto it = m_passMap.find(name);
    if (it != m_passMap.end()) {
        return it->second;
    }
    return nullptr;
}

bool RenderGraph::Setup() {
    // Validate the graph
    if (!Validate()) {
        return false;
    }
    
    // Resolve resource dependencies
    if (!ResolveDependencies()) {
        return false;
    }
    
    // Setup each pass
    for (auto* pass : m_passes) {
        pass->SetupResources(nullptr);  // TODO: Implement proper builder integration
    }
    
    m_valid = true;
    return true;
}

void RenderGraph::Execute(const RenderGraphContext& context) {
    if (!m_valid) {
        return;
    }
    
    // Update resource frame tracking
    m_resources->UpdateFrame(context.frameIndex);
    
    // Execute each pass in order
    for (auto* pass : m_passes) {
        pass->Execute(context.cmdList, context.resources);
    }
}

bool RenderGraph::Validate() {
    // Check that all passes have unique names
    if (m_passes.size() != m_passMap.size()) {
        m_error = "Duplicate pass names detected";
        return false;
    }
    
    // Check that each pass has a valid description
    for (auto* pass : m_passes) {
        if (pass->GetName().empty()) {
            m_error = "Pass has empty name";
            return false;
        }
    }
    
    // TODO: Add more validation (resource dependencies, etc.)
    return true;
}

bool RenderGraph::ResolveDependencies() {
    // Resolve resource dependencies between passes
    // TODO: Implement proper dependency resolution
    return true;
}

// ============================================================================
// Render Graph Registry
// ============================================================================

RenderGraphRegistry& RenderGraphRegistry::GetInstance() {
    static RenderGraphRegistry instance;
    return instance;
}

RenderGraphRegistry::~RenderGraphRegistry() {
    Clear();
}

void RenderGraphRegistry::RegisterGraph(RenderGraph* graph) {
    if (graph) {
        m_graphs.push_back(graph);
        m_graphMap[graph->GetName()] = graph;
    }
}

void RenderGraphRegistry::UnregisterGraph(const string& name) {
    auto it = m_graphMap.find(name);
    if (it != m_graphMap.end()) {
        auto graphIt = std::find(m_graphs.begin(), m_graphs.end(), it->second);
        if (graphIt != m_graphs.end()) {
            m_graphs.erase(graphIt);
        }
        m_graphMap.erase(it);
    }
}

RenderGraph* RenderGraphRegistry::GetGraph(const string& name) const {
    auto it = m_graphMap.find(name);
    if (it != m_graphMap.end()) {
        return it->second;
    }
    return nullptr;
}

void RenderGraphRegistry::Clear() {
    for (auto* graph : m_graphs) {
        delete graph;
    }
    m_graphs.clear();
    m_graphMap.clear();
}

} // namespace RenderGraph
} // namespace Luma