#pragma once

#include "Luma/Core/Types.h"
#include <string>

namespace Luma {

// ============================================================================
// Luma Texture Binary Format (.ltex)
// ============================================================================

// File header (64 bytes)
struct LumaTextureHeader {
    char magic[4] = {'L', 'T', 'E', 'X'};  // Magic bytes
    u32 version = 1;                          // Format version
    u32 flags = 0;                           // Compression flags, etc.
    u32 width = 0;                            // Texture width
    u32 height = 0;                           // Texture height
    u32 depth = 1;                            // Texture depth (1 for 2D)
    u32 mipLevels = 1;                        // Number of mip levels
    u32 arraySize = 1;                        // Array size (1 for non-array)
    u32 format = 0;                           // Pixel format
    u32 dataType = 0;                         // Data type
    u32 totalSize = 0;                        // Total data size including mips
    u32 reserved[6] = {0};                    // Reserved for future use
};

// Pixel formats
enum class TextureFormat : u32 {
    // 8-bit formats
    R8 = 0,
    RG8,
    RGB8,
    RGBA8,
    
    // sRGB formats
    SRGB8,
    SRGBA8,
    
    // 16-bit formats
    R16,
    RG16,
    RGB16,
    RGBA16,
    
    // Floating point formats
    R16F,
    RG16F,
    RGB16F,
    RGBA16F,
    
    R32F,
    RG32F,
    RGB32F,
    RGBA32F,
    
    // Compressed formats
    BC1,
    BC1_SRGB,
    BC2,
    BC2_SRGB,
    BC3,
    BC3_SRGB,
    BC4,
    BC5,
    BC6H,
    BC6H_SFLOAT,
    BC7,
    BC7_SRGB,
    
    // Depth formats
    D16,
    D24_S8,
    D32F,
    D32F_S8
};

// Data types
enum class TextureDataType : u32 {
    UNorm = 0,      // Unsigned normalized [0, 1]
    SNorm,          // Signed normalized [-1, 1]
    UInt,           // Unsigned integer
    SInt,           // Signed integer
    Float           // Floating point
};

// Compression flags
enum class TextureCompression : u32 {
    None = 0,
    BC1 = 1,
    BC3 = 2,
    BC7 = 4,
    ASTC = 8,
    ETC2 = 16
};

// Mip level descriptor
struct LumaMipLevel {
    u32 width;
    u32 height;
    u32 depth;
    u32 dataSize;      // Size of this mip level in bytes
    u32 dataOffset;   // Offset from start of data section
};

// Texture metadata
struct LumaTextureMetadata {
    std::string sourcePath;       // Original source file path
    u64 sourceHash;              // Hash of source file
    f32 sourceMtime;             // Modification time of source
    std::string importSettings;  // Import settings JSON
    bool isNormalMap = false;    // Normal map flag
    bool isSRGB = false;          // sRGB color space flag
    bool generateMipmaps = true;  // Mipmap generation flag
};

} // namespace Luma