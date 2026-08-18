#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "Luma/Core/Types.h"
#include "Luma/RHI/RHIResources.h"
#include "Luma/RHI/RHICommandList.h"

// Render graph system. Inspired by UE5's render graph system but adapted
// for Luma's simpler architecture. Provides a framework for managing render
// passes and resource dependencies efficiently.

namespace Luma {
namespace RenderGraph {

using std::string;
using std::vector;
using std::unordered_map;
using std::unique_ptr;

// Forward declarations
class RenderPass;
class RenderGraphResources;
class RenderGraphBuilder;

// ============================================================================
// Render Pass
// ============================================================================

// Render pass type
enum class ERenderPassType : u32 {
    Graphics,       // Graphics rendering pass
    Compute,        // Compute shader pass
    Copy,           // Copy/resolve pass
    Present,        // Present pass
};

// Render pass flags
enum class ERenderPassFlags : u32 {
    None = 0,
    BeginFrame = 1 << 0,
    EndFrame = 1 << 1,
    ReadOnly = 1 << 2,
    SkipCulling = 1 << 3,
};
inline ERenderPassFlags operator|(ERenderPassFlags a, ERenderPassFlags b) {
    return static_cast<ERenderPassFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}

// Render pass description
struct RenderPassDesc {
    string name;
    ERenderPassType type = ERenderPassType::Graphics;
    ERenderPassFlags flags = ERenderPassFlags::None;
    
    // Input resources
    vector<string> inputTextures;
    vector<string> inputBuffers;
    
    // Output resources
    vector<string> outputTextures;
    vector<string> outputBuffers;
    
    // Render target outputs
    vector<string> renderTargets;
    string depthStencilTarget;
    
    // Shader dependencies
    string vertexShader;
    string fragmentShader;
    string computeShader;
};

// Render pass interface
class RenderPass {
public:
    virtual ~RenderPass() = default;
    
    // Get pass description
    const RenderPassDesc& GetDesc() const { return m_desc; }
    
    // Execute the pass
    virtual void Execute(RHI::RHICommandList* cmdList, RenderGraphResources* resources) = 0;
    
    // Setup pass resources
    virtual void SetupResources(RenderGraphBuilder* builder) = 0;
    
    // Get pass debug name
    const string& GetName() const { return m_desc.name; }
    
protected:
    RenderPassDesc m_desc;
};

// ============================================================================
// Render Graph Resources
// ============================================================================

// Resource state tracking
struct ResourceState {
    RHI::EResourceState state = RHI::EResourceState::Common;
    u32 lastUsedFrame = 0;
    bool isTransient = false;
};

// Render graph resource tracking
class RenderGraphResources {
public:
    RenderGraphResources();
    ~RenderGraphResources();
    
    // Register texture resource
    void RegisterTexture(const string& name, RHI::RHITexture* texture, RHI::EResourceState initialState = RHI::EResourceState::Common);
    
    // Register buffer resource
    void RegisterBuffer(const string& name, RHI::RHIBuffer* buffer, RHI::EResourceState initialState = RHI::EResourceState::Common);
    
    // Get texture resource
    RHI::RHITexture* GetTexture(const string& name) const;
    
    // Get buffer resource
    RHI::RHIBuffer* GetBuffer(const string& name) const;
    
    // Get resource state
    const ResourceState* GetResourceState(const string& name) const;
    
    // Set resource state
    void SetResourceState(const string& name, RHI::EResourceState state);
    
    // Check if resource exists
    bool HasTexture(const string& name) const;
    bool HasBuffer(const string& name) const;
    
    // Get all textures
    const unordered_map<string, RHI::RHITexture*>& GetTextures() const { return m_textures; }
    
    // Get all buffers
    const unordered_map<string, RHI::RHIBuffer*>& GetBuffers() const { return m_buffers; }
    
    // Clear all resources
    void Clear();
    
    // Update frame tracking
    void UpdateFrame(u32 frame);
    
private:
    unordered_map<string, RHI::RHITexture*> m_textures;
    unordered_map<string, RHI::RHIBuffer*> m_buffers;
    unordered_map<string, ResourceState> m_resourceStates;
    u32 m_currentFrame = 0;
};

// ============================================================================
// Render Graph
// ============================================================================

// Render graph execution context
struct RenderGraphContext {
    RHI::RHICommandList* cmdList = nullptr;
    RenderGraphResources* resources = nullptr;
    u32 frameIndex = 0;
    u32 width = 0;
    u32 height = 0;
};

// Render graph interface
class RenderGraph {
public:
    RenderGraph(const string& name);
    ~RenderGraph();
    
    // Get graph name
    const string& GetName() const { return m_name; }
    
    // Add render pass
    void AddPass(RenderPass* pass);
    
    // Remove render pass
    void RemovePass(const string& name);
    
    // Get render pass by name
    RenderPass* GetPass(const string& name) const;
    
    // Get all passes
    const vector<RenderPass*>& GetPasses() const { return m_passes; }
    
    // Setup the graph
    bool Setup();
    
    // Execute the graph
    void Execute(const RenderGraphContext& context);
    
    // Set graph dimensions
    void SetDimensions(u32 width, u32 height) {
        m_width = width;
        m_height = height;
    }
    
    // Get graph dimensions
    u32 GetWidth() const { return m_width; }
    u32 GetHeight() const { return m_height; }
    
    // Get resources
    RenderGraphResources* GetResources() const { return m_resources; }
    
    // Check if graph is valid
    bool IsValid() const { return m_valid; }
    
    // Get error message
    const string& GetError() const { return m_error; }
    
private:
    string m_name;
    vector<RenderPass*> m_passes;
    unordered_map<string, RenderPass*> m_passMap;
    RenderGraphResources* m_resources;
    u32 m_width = 0;
    u32 m_height = 0;
    bool m_valid = false;
    string m_error;
    
    // Validate graph
    bool Validate();
    
    // Resolve resource dependencies
    bool ResolveDependencies();
};

// ============================================================================
// Render Graph Registry
// ============================================================================

// Global registry for render graphs
class RenderGraphRegistry {
public:
    static RenderGraphRegistry& GetInstance();
    
    // Register render graph
    void RegisterGraph(RenderGraph* graph);
    
    // Unregister render graph
    void UnregisterGraph(const string& name);
    
    // Get render graph by name
    RenderGraph* GetGraph(const string& name) const;
    
    // Get all graphs
    const vector<RenderGraph*>& GetAllGraphs() const { return m_graphs; }
    
    // Clear all graphs
    void Clear();
    
private:
    RenderGraphRegistry() = default;
    ~RenderGraphRegistry();
    
    vector<RenderGraph*> m_graphs;
    unordered_map<string, RenderGraph*> m_graphMap;
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Create render graph
inline RenderGraph* CreateRenderGraph(const string& name) {
    auto* graph = new RenderGraph(name);
    RenderGraphRegistry::GetInstance().RegisterGraph(graph);
    return graph;
}

// Destroy render graph
inline void DestroyRenderGraph(RenderGraph* graph) {
    RenderGraphRegistry::GetInstance().UnregisterGraph(graph->GetName());
    delete graph;
}

// Get render graph by name
inline RenderGraph* GetRenderGraph(const string& name) {
    return RenderGraphRegistry::GetInstance().GetGraph(name);
}

} // namespace RenderGraph
} // namespace Luma