#pragma once

#include <memory>
#include <vector>
#include <string>

#include "Luma/Core/Types.h"
#include "Luma/RHI/RHIResources.h"
#include "Luma/RHI/RHITypes.h"

// RHI pipeline state objects. Inspired by UE5's pipeline state system but
// simplified for Luma's architecture. Pipeline states encapsulate all rendering
// state (shaders, blend state, rasterizer state, etc.) for efficient GPU switching.

namespace Luma {
namespace RHI {

using std::vector;
using std::string;

// Forward declarations
class RHIShader;
class RHIBuffer;

// ============================================================================
// Rasterizer State
// ============================================================================

// Rasterizer state description
struct RasterizerDesc {
    EFillMode fillMode = EFillMode::Solid;
    ECullMode cullMode = ECullMode::Back;
    EFrontFace frontFace = EFrontFace::CounterClockwise;
    f32 depthBias = 0.0f;
    f32 depthBiasClamp = 0.0f;
    f32 slopeScaledDepthBias = 0.0f;
    bool depthClipEnable = true;
    bool conservativeRasterEnable = false;
    
    // Debug name
    string name;
};

// Rasterizer state interface
class RHIRasterizerState {
public:
    virtual ~RHIRasterizerState() = default;
    
    const RasterizerDesc& GetDesc() const { return m_desc; }
    
protected:
    RasterizerDesc m_desc;
};

// ============================================================================
// Blend State
// ============================================================================

// Render target blend description
struct RenderTargetBlendDesc {
    bool blendEnable = false;
    bool logicOpEnable = false;
    EBlendFactor srcBlend = EBlendFactor::One;
    EBlendFactor dstBlend = EBlendFactor::Zero;
    EBlendOp blendOp = EBlendOp::Add;
    EBlendFactor srcBlendAlpha = EBlendFactor::One;
    EBlendFactor dstBlendAlpha = EBlendFactor::Zero;
    EBlendOp blendOpAlpha = EBlendOp::Add;
    EColorWriteMask writeMask = EColorWriteMask::All;
};

// Blend state description
struct BlendDesc {
    bool alphaToCoverageEnable = false;
    bool independentBlendEnable = false;
    RenderTargetBlendDesc renderTarget[8];  // Up to 8 render targets
    
    // Debug name
    string name;
};

// Blend state interface
class RHIBlendState {
public:
    virtual ~RHIBlendState() = default;
    
    const BlendDesc& GetDesc() const { return m_desc; }
    
protected:
    BlendDesc m_desc;
};

// ============================================================================
// Depth-Stencil State
// ============================================================================

// Depth-stencil operation description
struct DepthStencilOpDesc {
    EStencilOp stencilFailOp = EStencilOp::Keep;
    EStencilOp stencilDepthFailOp = EStencilOp::Keep;
    EStencilOp stencilPassOp = EStencilOp::Keep;
    ECompareFunc stencilFunc = ECompareFunc::Always;
};

// Depth-stencil state description
struct DepthStencilDesc {
    bool depthEnable = true;
    bool depthWriteEnable = true;
    ECompareFunc depthFunc = ECompareFunc::Less;
    bool stencilEnable = false;
    u8 stencilReadMask = 0xFF;
    u8 stencilWriteMask = 0xFF;
    DepthStencilOpDesc frontFace;
    DepthStencilOpDesc backFace;
    
    // Debug name
    string name;
};

// Depth-stencil state interface
class RHIDepthStencilState {
public:
    virtual ~RHIDepthStencilState() = default;
    
    const DepthStencilDesc& GetDesc() const { return m_desc; }
    
protected:
    DepthStencilDesc m_desc;
};

// ============================================================================
// Input Layout
// ============================================================================

// Input element description
struct InputElementDesc {
    const char* semanticName;
    u32 semanticIndex;
    ETextureFormat format;
    u32 inputSlot;
    u32 alignedByteOffset;
    EBufferUsage inputSlotClass;
    u32 instanceDataStepRate;
};

// Input layout description
struct InputLayoutDesc {
    const InputElementDesc* elements = nullptr;
    u32 elementCount = 0;
    
    // Debug name
    string name;
};

// Input layout interface
class RHIInputLayout {
public:
    virtual ~RHIInputLayout() = default;
    
    const InputLayoutDesc& GetDesc() const { return m_desc; }
    
protected:
    InputLayoutDesc m_desc;
};

// ============================================================================
// Graphics Pipeline State
// ============================================================================

// Graphics pipeline state description
struct GraphicsPipelineDesc {
    // Shaders
    RHIShader* vertexShader = nullptr;
    RHIShader* fragmentShader = nullptr;
    RHIShader* geometryShader = nullptr;
    RHIShader* tessControlShader = nullptr;
    RHIShader* tessEvalShader = nullptr;
    
    // Input layout
    InputLayoutDesc inputLayout;
    
    // Pipeline states
    RasterizerDesc rasterizer;
    BlendDesc blend;
    DepthStencilDesc depthStencil;
    
    // Primitive topology
    EPrimitiveTopology primitiveTopology = EPrimitiveTopology::TriangleList;
    
    // Render target formats
    ETextureFormat renderTargetFormats[8] = {ETextureFormat::Unknown};
    u32 numRenderTargets = 0;
    ETextureFormat depthStencilFormat = ETextureFormat::Unknown;
    
    // Sample count
    ESampleCount sampleCount = ESampleCount::Count1;
    
    // Debug name
    string name;
};

// Graphics pipeline state interface
class RHIGraphicsPipelineState {
public:
    virtual ~RHIGraphicsPipelineState() = default;
    
    const GraphicsPipelineDesc& GetDesc() const { return m_desc; }
    
protected:
    GraphicsPipelineDesc m_desc;
};

// ============================================================================
// Compute Pipeline State
// ============================================================================

// Compute pipeline state description
struct ComputePipelineDesc {
    RHIShader* computeShader = nullptr;
    
    // Debug name
    string name;
};

// Compute pipeline state interface
class RHIComputePipelineState {
public:
    virtual ~RHIComputePipelineState() = default;
    
    const ComputePipelineDesc& GetDesc() const { return m_desc; }
    
protected:
    ComputePipelineDesc m_desc;
};

// ============================================================================
// Pipeline State Factory
// ============================================================================

// Pipeline state creation interface (implemented by RHI backends)
class RHIPipelineStateFactory {
public:
    virtual ~RHIPipelineStateFactory() = default;
    
    // Rasterizer state
    virtual RHIRasterizerState* CreateRasterizerState(const RasterizerDesc& desc) = 0;
    virtual void DestroyRasterizerState(RHIRasterizerState* state) = 0;
    
    // Blend state
    virtual RHIBlendState* CreateBlendState(const BlendDesc& desc) = 0;
    virtual void DestroyBlendState(RHIBlendState* state) = 0;
    
    // Depth-stencil state
    virtual RHIDepthStencilState* CreateDepthStencilState(const DepthStencilDesc& desc) = 0;
    virtual void DestroyDepthStencilState(RHIDepthStencilState* state) = 0;
    
    // Input layout
    virtual RHIInputLayout* CreateInputLayout(const InputLayoutDesc& desc) = 0;
    virtual void DestroyInputLayout(RHIInputLayout* layout) = 0;
    
    // Graphics pipeline state
    virtual RHIGraphicsPipelineState* CreateGraphicsPipelineState(const GraphicsPipelineDesc& desc) = 0;
    virtual void DestroyGraphicsPipelineState(RHIGraphicsPipelineState* pso) = 0;
    
    // Compute pipeline state
    virtual RHIComputePipelineState* CreateComputePipelineState(const ComputePipelineDesc& desc) = 0;
    virtual void DestroyComputePipelineState(RHIComputePipelineState* pso) = 0;
};

} // namespace RHI
} // namespace Luma