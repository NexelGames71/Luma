#pragma once

#include <cstdint>
#include <string>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"

// Core RHI type definitions and enums. This file defines the fundamental types
// used throughout the rendering hardware interface, inspired by UE5's RHI but
// adapted for Luma's simpler architecture.

namespace Luma {
namespace RHI {

using namespace Luma::Math;

// Forward declarations
class RHIResource;
class RHIBuffer;
class RHITexture;
class RHIShader;
class RHIPipelineState;

// ============================================================================
// Resource Types
// ============================================================================

// GPU buffer usage flags
enum class EBufferUsage : u32 {
    None = 0,
    Vertex = 1 << 0,        // Vertex buffer
    Index = 1 << 1,         // Index buffer
    Uniform = 1 << 2,       // Uniform/constant buffer
    Structured = 1 << 3,    // Structured buffer (shader storage)
    Argument = 1 << 4,      // Indirect argument buffer
    TransferSrc = 1 << 5,   // Transfer source (copy from)
    TransferDst = 1 << 6,   // Transfer destination (copy to)
};
inline EBufferUsage operator|(EBufferUsage a, EBufferUsage b) {
    return static_cast<EBufferUsage>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline EBufferUsage operator&(EBufferUsage a, EBufferUsage b) {
    return static_cast<EBufferUsage>(static_cast<u32>(a) & static_cast<u32>(b));
}

// CPU access flags for buffer mapping
enum class EBufferCPUAccess : u32 {
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1,
    ReadWrite = Read | Write,
};

// Texture dimensionality
enum class ETextureDimension : u8 {
    Texture1D,
    Texture2D,
    Texture3D,
    TextureCube,
};

// Texture format
enum class ETextureFormat : u32 {
    Unknown,
    
    // Color formats
    R8_UNORM,
    R8_SNORM,
    R8_UINT,
    R8_SINT,
    
    R16_UNORM,
    R16_SNORM,
    R16_UINT,
    R16_SINT,
    R16_FLOAT,
    
    R8G8_UNORM,
    R8G8_SNORM,
    R8G8_UINT,
    R8G8_SINT,
    
    R16G16_UNORM,
    R16G16_SNORM,
    R16G16_UINT,
    R16G16_SINT,
    R16G16_FLOAT,
    
    R32_UINT,
    R32_SINT,
    R32_FLOAT,
    
    R8G8B8A8_UNORM,  // Common UI format
    R8G8B8A8_SRGB,
    R8G8B8A8_UINT,
    R8G8B8A8_SINT,
    
    R16G16B16A16_UNORM,
    R16G16B16A16_SNORM,
    R16G16B16A16_UINT,
    R16G16B16A16_SINT,
    R16G16B16A16_FLOAT,
    
    R32G32_UINT,
    R32G32_SINT,
    R32G32_FLOAT,
    
    R32G32B32A32_UINT,
    R32G32B32A32_SINT,
    R32G32B32A32_FLOAT,
    
    // Depth-stencil formats
    D16_UNORM,
    D24_UNORM_S8_UINT,
    D32_FLOAT,
    D32_FLOAT_S8_UINT,
    
    // Compressed formats
    BC1_UNORM,
    BC1_SRGB,
    BC3_UNORM,
    BC3_SRGB,
    BC4_UNORM,
    BC4_SNORM,
    BC5_UNORM,
    BC5_SNORM,
    BC6H_UFLOAT,
    BC6H_SFLOAT,
    BC7_UNORM,
    BC7_SRGB,
};

// Texture usage flags
enum class ETextureUsage : u32 {
    None = 0,
    ShaderResource = 1 << 0,     // Sampled in shaders
    RenderTarget = 1 << 1,       // Color render target
    DepthStencil = 1 << 2,       // Depth-stencil target
    UnorderedAccess = 1 << 3,    // UAV (shader storage)
    TransferSrc = 1 << 4,        // Transfer source
    TransferDst = 1 << 5,        // Transfer destination
};
inline ETextureUsage operator|(ETextureUsage a, ETextureUsage b) {
    return static_cast<ETextureUsage>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline ETextureUsage operator&(ETextureUsage a, ETextureUsage b) {
    return static_cast<ETextureUsage>(static_cast<u32>(a) & static_cast<u32>(b));
}

// Texture creation flags
enum class ETextureFlags : u32 {
    None = 0,
    GenerateMips = 1 << 0,       // Generate mipmaps on creation
    RenderTargetable = 1 << 1,   // Can be used as render target
    DepthStencilTargetable = 1 << 2,  // Can be used as depth-stencil
    ShaderResource = 1 << 3,     // Can be sampled in shaders
    Cubemap = 1 << 4,            // Cubemap texture
    Array = 1 << 5,              // Texture array
    Volume = 1 << 6,             // 3D texture
};
inline ETextureFlags operator|(ETextureFlags a, ETextureFlags b) {
    return static_cast<ETextureFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline ETextureFlags operator&(ETextureFlags a, ETextureFlags b) {
    return static_cast<ETextureFlags>(static_cast<u32>(a) & static_cast<u32>(b));
}

// Sample count for MSAA
enum class ESampleCount : u32 {
    Count1 = 1,
    Count2 = 2,
    Count4 = 4,
    Count8 = 8,
};

// ============================================================================
// Shader Types
// ============================================================================

// Shader stages
enum class EShaderStage : u32 {
    Vertex = 1 << 0,
    Fragment = 1 << 1,
    Geometry = 1 << 2,
    TessControl = 1 << 3,
    TessEvaluation = 1 << 4,
    Compute = 1 << 5,
};
inline EShaderStage operator|(EShaderStage a, EShaderStage b) {
    return static_cast<EShaderStage>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline EShaderStage operator&(EShaderStage a, EShaderStage b) {
    return static_cast<EShaderStage>(static_cast<u32>(a) & static_cast<u32>(b));
}

// Shader language
enum class EShaderLanguage {
    GLSL,
    HLSL,
};

// ============================================================================
// Pipeline State
// ============================================================================

// Primitive topology
enum class EPrimitiveTopology {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    LineListWithAdjacency,
    LineStripWithAdjacency,
    TriangleListWithAdjacency,
    TriangleStripWithAdjacency,
};

// Fill mode
enum class EFillMode {
    Solid,
    Wireframe,
};

// Cull mode
enum class ECullMode {
    None,
    Front,
    Back,
};

// Front face winding
enum class EFrontFace {
    CounterClockwise,
    Clockwise,
};

// Comparison function
enum class ECompareFunc {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

// Stencil operation
enum class EStencilOp {
    Keep,
    Zero,
    Replace,
    IncrementAndClamp,
    DecrementAndClamp,
    Invert,
    IncrementAndWrap,
    DecrementAndWrap,
};

// Blend factor
enum class EBlendFactor {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor,
    ConstantAlpha,
    OneMinusConstantAlpha,
};

// Blend operation
enum class EBlendOp {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

// Color write mask
enum class EColorWriteMask : u8 {
    None = 0,
    Red = 1 << 0,
    Green = 1 << 1,
    Blue = 1 << 2,
    Alpha = 1 << 3,
    All = Red | Green | Blue | Alpha,
};
inline EColorWriteMask operator|(EColorWriteMask a, EColorWriteMask b) {
    return static_cast<EColorWriteMask>(static_cast<u8>(a) | static_cast<u8>(b));
}
inline EColorWriteMask operator&(EColorWriteMask a, EColorWriteMask b) {
    return static_cast<EColorWriteMask>(static_cast<u8>(a) & static_cast<u8>(b));
}

// ============================================================================
// Resource View Types
// ============================================================================

// Shader resource view dimension
enum class ESRVDimension {
    Unknown,
    Buffer,
    Texture1D,
    Texture1DArray,
    Texture2D,
    Texture2DArray,
    Texture2DMS,
    Texture2DMSArray,
    Texture3D,
    TextureCube,
    TextureCubeArray,
};

// Unordered access view dimension
enum class EUAVDimension {
    Unknown,
    Buffer,
    Texture1D,
    Texture1DArray,
    Texture2D,
    Texture2DArray,
    Texture3D,
};

// Render target view dimension
enum class ERTVDimension {
    Unknown,
    Buffer,
    Texture1D,
    Texture1DArray,
    Texture2D,
    Texture2DArray,
    Texture2DMS,
    Texture2DMSArray,
    Texture3D,
};

// Depth-stencil view dimension
enum class EDSVDimension {
    Unknown,
    Texture1D,
    Texture1DArray,
    Texture2D,
    Texture2DArray,
    Texture2DMS,
    Texture2DMSArray,
};

// ============================================================================
// Resource States
// ============================================================================

// Resource state for synchronization
enum class EResourceState : u32 {
    Undefined,
    Common,
    VertexBuffer,
    IndexBuffer,
    ConstantBuffer,
    ShaderResource,
    UnorderedAccess,
    RenderTarget,
    DepthWrite,
    DepthRead,
    CopyDest,
    CopySource,
    ResolveDest,
    ResolveSource,
    Present,
    GenericRead,
};

// ============================================================================
// Texture and View Descriptors
// ============================================================================

// Texture subresource range
struct TextureSubresourceRange {
    u32 baseMipLevel = 0;
    u32 mipLevels = 1;
    u32 baseArrayLayer = 0;
    u32 arrayLayers = 1;
};

// Texture subresource layers
struct TextureSubresourceLayers {
    u32 mipLevel = 0;
    u32 baseArrayLayer = 0;
    u32 arrayLayers = 1;
};

// Texture copy region
struct TextureCopyRegion {
    u32 srcSubresource = 0;
    Vec3 srcOffset{0.0f, 0.0f, 0.0f};
    u32 dstSubresource = 0;
    Vec3 dstOffset{0.0f, 0.0f, 0.0f};
    Vec3 extent{0.0f, 0.0f, 0.0f};
};

// Buffer to texture copy
struct BufferToTextureCopy {
    u64 bufferOffset = 0;
    u32 bufferRowLength = 0;
    u32 bufferImageHeight = 0;
    TextureSubresourceLayers textureSubresource;
    Vec3 textureOffset{0.0f, 0.0f, 0.0f};
    Vec3 textureExtent{0.0f, 0.0f, 0.0f};
};

// ============================================================================
// Utility Functions
// ============================================================================

// Get format size in bytes
u32 GetFormatSize(ETextureFormat format);

// Get format component count
u32 GetFormatComponentCount(ETextureFormat format);

// Check if format is depth-stencil
bool IsDepthStencilFormat(ETextureFormat format);

// Check if format is compressed
bool IsCompressedFormat(ETextureFormat format);

// Check if format is sRGB
bool IsSRGBFormat(ETextureFormat format);

// Get corresponding non-sRGB format
ETextureFormat GetNonSRGBFormat(ETextureFormat format);

// Get corresponding sRGB format
ETextureFormat GetSRGBFormat(ETextureFormat format);

} // namespace RHI
} // namespace Luma