#pragma once

#include "Luma/Core/Types.h"
#include "Luma/Asset/AssetId.h"
#include <vector>
#include <string>

namespace Luma {

// ============================================================================
// Luma Mesh Binary Format (.lmesh)
// ============================================================================

// File header (32 bytes)
struct LumaMeshHeader {
    char magic[4] = {'L', 'M', 'E', 'S'};  // Magic bytes
    u32 version = 1;                          // Format version
    u32 flags = 0;                           // Compression flags, etc.
    u32 vertexCount = 0;                      // Total vertices
    u32 indexCount = 0;                       // Total indices
    u32 submeshCount = 0;                     // Number of submeshes
    u32 vertexStride = 0;                     // Bytes per vertex
    u32 indexType = 0;                        // 0 = u16, 1 = u32
    u32 reserved[3] = {0};                    // Reserved for future use
};

// Vertex attribute types
enum class VertexAttribute : u32 {
    Position = 0,
    Normal,
    Tangent,
    TexCoord0,
    TexCoord1,
    Color,
    BoneIndices,
    BoneWeights,
    Count
};

// Vertex attribute descriptor
struct VertexAttributeDesc {
    VertexAttribute type;
    u32 offset;      // Byte offset in vertex
    u32 componentCount;  // 1, 2, 3, or 4
    u32 dataType;    // 0 = f32, 1 = u8, 2 = u16
};

// Compression flags
enum class MeshCompression : u32 {
    None = 0,
    LZ4 = 1,
    Zstd = 2,
    QuantizePosition = 4,
    QuantizeNormal = 8,
    QuantizeTexCoord = 16
};

} // namespace Luma