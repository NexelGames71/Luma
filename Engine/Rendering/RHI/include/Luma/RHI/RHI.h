#pragma once

#include <cstring>

// Main RHI header that includes all RHI functionality. This is the primary
// entry point for the Rendering Hardware Interface system.

#include "Luma/RHI/RHITypes.h"
#include "Luma/RHI/RHIResources.h"
#include "Luma/RHI/RHICommandList.h"
#include "Luma/RHI/RHIContext.h"
#include "Luma/RHI/RHIPipeline.h"
#include "Luma/RHI/RHIShader.h"
#include "Luma/RHI/RHIBoundShaderState.h"

// ============================================================================
// RHI Initialization and Global State
// ============================================================================

namespace Luma {
namespace RHI {

// Initialize the RHI system with a specific backend
bool InitializeRHI(const char* backendName, const RHIInitDesc& desc = RHIInitDesc());

// Shutdown the RHI system
void ShutdownRHI();

// Get the global RHI device
RHIDevice* GetRHIDevice();

// Get the global RHI context
RHIContext* GetRHIContext();

// Check if RHI is initialized
bool IsRHIInitialized();

// ============================================================================
// Convenience Functions
// ============================================================================

// Create a vertex buffer
inline RHIBuffer* CreateVertexBuffer(u64 size, const void* data = nullptr) {
    BufferDesc desc;
    desc.size = size;
    desc.usage = EBufferUsage::Vertex;
    desc.initialData = data;
    desc.cpuAccess = EBufferCPUAccess::None;
    return GetRHIContext()->GetResourceFactory()->CreateBuffer(desc);
}

// Create an index buffer
inline RHIBuffer* CreateIndexBuffer(u64 size, const void* data = nullptr) {
    BufferDesc desc;
    desc.size = size;
    desc.usage = EBufferUsage::Index;
    desc.initialData = data;
    desc.cpuAccess = EBufferCPUAccess::None;
    return GetRHIContext()->GetResourceFactory()->CreateBuffer(desc);
}

// Create a uniform buffer
inline RHIBuffer* CreateUniformBuffer(u64 size, const void* data = nullptr) {
    BufferDesc desc;
    desc.size = size;
    desc.usage = EBufferUsage::Uniform;
    desc.initialData = data;
    desc.cpuAccess = EBufferCPUAccess::Write;
    return GetRHIContext()->GetResourceFactory()->CreateBuffer(desc);
}

// Create a texture
inline RHITexture* CreateTexture(u32 width, u32 height, ETextureFormat format, const void* data = nullptr) {
    TextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.format = format;
    desc.usage = ETextureUsage::ShaderResource | ETextureUsage::RenderTarget;
    desc.flags = ETextureFlags::ShaderResource | ETextureFlags::RenderTargetable;
    desc.initialData = data;
    return GetRHIContext()->GetResourceFactory()->CreateTexture(desc);
}

// Create a depth-stencil texture
inline RHITexture* CreateDepthStencilTexture(u32 width, u32 height, ETextureFormat format = ETextureFormat::D24_UNORM_S8_UINT) {
    TextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.format = format;
    desc.usage = ETextureUsage::DepthStencil;
    desc.flags = ETextureFlags::DepthStencilTargetable;
    return GetRHIContext()->GetResourceFactory()->CreateTexture(desc);
}

// Create a sampler
inline RHISamplerState* CreateSampler(ESamplerFilter filter = ESamplerFilter::Linear) {
    SamplerDesc desc;
    desc.filter = filter;
    return GetRHIContext()->GetResourceFactory()->CreateSampler(desc);
}

// ============================================================================
// Pipeline State Convenience Functions
// ============================================================================

// Note: Pipeline state creation requires RHIPipelineStateFactory which will be
// available after backend implementation. These functions will be implemented
// once the Vulkan backend is complete.

/*
// Create a default rasterizer state
inline RHIRasterizerState* CreateDefaultRasterizerState() {
    RasterizerDesc desc;
    desc.fillMode = EFillMode::Solid;
    desc.cullMode = ECullMode::Back;
    desc.frontFace = EFrontFace::CounterClockwise;
    desc.depthBias = 0.0f;
    desc.depthBiasClamp = 0.0f;
    desc.slopeScaledDepthBias = 0.0f;
    desc.depthClipEnable = true;
    desc.conservativeRasterEnable = false;
    return GetRHIDevice()->GetContext()->GetPipelineStateFactory()->CreateRasterizerState(desc);
}

// Create a default blend state
inline RHIBlendState* CreateDefaultBlendState() {
    BlendDesc desc;
    desc.alphaToCoverageEnable = false;
    desc.independentBlendEnable = false;
    for (u32 i = 0; i < 8; ++i) {
        desc.renderTarget[i].blendEnable = false;
        desc.renderTarget[i].writeMask = EColorWriteMask::All;
    }
    return GetRHIDevice()->GetContext()->GetPipelineStateFactory()->CreateBlendState(desc);
}

// Create a default depth-stencil state
inline RHIDepthStencilState* CreateDefaultDepthStencilState() {
    DepthStencilDesc desc;
    desc.depthEnable = true;
    desc.depthWriteEnable = true;
    desc.depthFunc = ECompareFunc::Less;
    desc.stencilEnable = false;
    desc.stencilReadMask = 0xFF;
    desc.stencilWriteMask = 0xFF;
    return GetRHIDevice()->GetContext()->GetPipelineStateFactory()->CreateDepthStencilState(desc);
}
*/

// ============================================================================
// Shader Convenience Functions
// ============================================================================

// Note: Shader creation requires RHIShaderFactory which will be available after
// backend implementation. These functions will be implemented once the Vulkan
// backend is complete.

/*
// Create a vertex shader from source
inline RHIShader* CreateVertexShader(const char* source, EShaderLanguage language = EShaderLanguage::GLSL) {
    ShaderDesc desc;
    desc.source = source;
    desc.sourceSize = std::strlen(source);
    desc.entryPoint = "main";
    desc.stage = EShaderStage::Vertex;
    desc.language = language;
    desc.flags = EShaderCompileFlags::Optimize;
    return GetRHIDevice()->GetContext()->GetShaderFactory()->CreateShader(desc);
}

// Create a fragment shader from source
inline RHIShader* CreateFragmentShader(const char* source, EShaderLanguage language = EShaderLanguage::GLSL) {
    ShaderDesc desc;
    desc.source = source;
    desc.sourceSize = std::strlen(source);
    desc.entryPoint = "main";
    desc.stage = EShaderStage::Fragment;
    desc.language = language;
    desc.flags = EShaderCompileFlags::Optimize;
    return GetRHIDevice()->GetContext()->GetShaderFactory()->CreateShader(desc);
}

// Create a compute shader from source
inline RHIShader* CreateComputeShader(const char* source, EShaderLanguage language = EShaderLanguage::GLSL) {
    ShaderDesc desc;
    desc.source = source;
    desc.sourceSize = std::strlen(source);
    desc.entryPoint = "main";
    desc.stage = EShaderStage::Compute;
    desc.language = language;
    desc.flags = EShaderCompileFlags::Optimize;
    return GetRHIDevice()->GetContext()->GetShaderFactory()->CreateShader(desc);
}
*/

// ============================================================================
// Utility Macros
// ============================================================================

// Scope guard for command list recording
#define RHI_CMD_LIST_SCOPE(cmdList) RHICommandListBuilder _cmdListBuilder(cmdList)
#define RHI_DEBUG_MARKER_SCOPE(cmdList, name) RHICommandListBuilder::DebugMarkerScope _debugMarker(cmdList, name)

// Resource management helpers
#define RHI_SAFE_DELETE_BUFFER(ptr) if (ptr) { GetRHIContext()->GetResourceFactory()->DestroyBuffer(ptr); ptr = nullptr; }
#define RHI_SAFE_DELETE_TEXTURE(ptr) if (ptr) { GetRHIContext()->GetResourceFactory()->DestroyTexture(ptr); ptr = nullptr; }
#define RHI_SAFE_DELETE_SRV(ptr) if (ptr) { GetRHIContext()->GetResourceFactory()->DestroyShaderResourceView(ptr); ptr = nullptr; }
#define RHI_SAFE_DELETE_UAV(ptr) if (ptr) { GetRHIContext()->GetResourceFactory()->DestroyUnorderedAccessView(ptr); ptr = nullptr; }
#define RHI_SAFE_DELETE_RTV(ptr) if (ptr) { GetRHIContext()->GetResourceFactory()->DestroyRenderTargetView(ptr); ptr = nullptr; }
#define RHI_SAFE_DELETE_DSV(ptr) if (ptr) { GetRHIContext()->GetResourceFactory()->DestroyDepthStencilView(ptr); ptr = nullptr; }
#define RHI_SAFE_DELETE_SAMPLER(ptr) if (ptr) { GetRHIContext()->GetResourceFactory()->DestroySampler(ptr); ptr = nullptr; }

// Pipeline state helpers (available after backend implementation)
#define RHI_SAFE_DELETE_RASTERIZER_STATE(ptr) if (ptr) { GetRHIContext()->GetPipelineStateFactory()->DestroyRasterizerState(ptr); ptr = nullptr; }
#define RHI_SAFE_DELETE_BLEND_STATE(ptr) if (ptr) { GetRHIContext()->GetPipelineStateFactory()->DestroyBlendState(ptr); ptr = nullptr; }
#define RHI_SAFE_DELETE_DEPTH_STENCIL_STATE(ptr) if (ptr) { GetRHIContext()->GetPipelineStateFactory()->DestroyDepthStencilState(ptr); ptr = nullptr; }
#define RHI_SAFE_DELETE_INPUT_LAYOUT(ptr) if (ptr) { GetRHIContext()->GetPipelineStateFactory()->DestroyInputLayout(ptr); ptr = nullptr; }
#define RHI_SAFE_DELETE_GRAPHICS_PSO(ptr) if (ptr) { GetRHIContext()->GetPipelineStateFactory()->DestroyGraphicsPipelineState(ptr); ptr = nullptr; }
#define RHI_SAFE_DELETE_COMPUTE_PSO(ptr) if (ptr) { GetRHIContext()->GetPipelineStateFactory()->DestroyComputePipelineState(ptr); ptr = nullptr; }

// Shader helpers (available after backend implementation)
#define RHI_SAFE_DELETE_SHADER(ptr) if (ptr) { GetRHIContext()->GetShaderFactory()->DestroyShader(ptr); ptr = nullptr; }

} // namespace RHI
} // namespace Luma