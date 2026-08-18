#include "Luma/Asset/TextureThumbnailRenderer.h"
#include "Luma/Core/Log.h"

#include <fstream>
#include <algorithm>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4505)
#endif
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace Luma {

// ============================================================================
// TextureThumbnailRenderer Implementation
// ============================================================================

bool TextureThumbnailRenderer::CanRender(const AssetId& assetId, const std::filesystem::path& nativePath) const {
    (void)assetId;
    
    if (!std::filesystem::exists(nativePath)) {
        return false;
    }
    
    std::string ext = nativePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return (ext == ".ltex" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".tga" || ext == ".bmp" || ext == ".webp" || ext == ".hdr");
}

bool TextureThumbnailRenderer::RenderThumbnail(const AssetId& assetId,
                                                 const std::filesystem::path& nativePath,
                                                 const std::filesystem::path& outputPath,
                                                 u32 width,
                                                 u32 height) {
    (void)assetId;
    
    // Load texture
    std::vector<u8> pixels;
    u32 texWidth, texHeight, channels;
    
    if (!LoadTexture(nativePath, pixels, texWidth, texHeight, channels)) {
        LUMA_LOG_ERROR("TextureThumbnailRenderer", "Failed to load texture: {}", nativePath.string());
        return false;
    }
    
    // Resize to thumbnail size
    std::vector<u8> thumbnailPixels;
    if (!ResizeTexture(pixels, texWidth, texHeight, channels, thumbnailPixels, width, height)) {
        LUMA_LOG_ERROR("TextureThumbnailRenderer", "Failed to resize texture");
        return false;
    }
    
    // Convert to RGBA8 if needed
    std::vector<u8> rgbaPixels;
    if (channels != 4) {
        if (!ConvertToRGBA8(thumbnailPixels, width, height, channels, rgbaPixels)) {
            LUMA_LOG_ERROR("TextureThumbnailRenderer", "Failed to convert to RGBA8");
            return false;
        }
    } else {
        rgbaPixels = std::move(thumbnailPixels);
    }
    
    // Apply checkerboard for textures with alpha (simplified - just check if texture might be transparent)
    bool hasAlpha = (channels == 4);
    if (hasAlpha) {
        // Blend with checkerboard
        const auto& checkerboard = ThumbnailManager::Get().GetCheckerboardTexture();
        u32 checkerSize = 32;
        
        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                u32 offset = (y * width + x) * 4;
                u8 alpha = rgbaPixels[offset + 3];
                
                if (alpha < 255) {
                    // Sample checkerboard
                    u32 cx = x % checkerSize;
                    u32 cy = y % checkerSize;
                    u32 checkerOffset = (cy * checkerSize + cx) * 4;
                    
                    // Blend
                    f32 blend = alpha / 255.0f;
                    rgbaPixels[offset + 0] = static_cast<u8>(rgbaPixels[offset + 0] * blend + checkerboard[checkerOffset + 0] * (1.0f - blend));
                    rgbaPixels[offset + 1] = static_cast<u8>(rgbaPixels[offset + 1] * blend + checkerboard[checkerOffset + 1] * (1.0f - blend));
                    rgbaPixels[offset + 2] = static_cast<u8>(rgbaPixels[offset + 2] * blend + checkerboard[checkerOffset + 2] * (1.0f - blend));
                }
            }
        }
    }
    
    // Write PNG
    int success = stbi_write_png(outputPath.string().c_str(),
                                  static_cast<int>(width),
                                  static_cast<int>(height),
                                  4,
                                  rgbaPixels.data(),
                                  static_cast<int>(width * 4));
    
    if (!success) {
        LUMA_LOG_ERROR("TextureThumbnailRenderer", "Failed to write thumbnail: {}", outputPath.string());
        return false;
    }
    
    LUMA_LOG_INFO("TextureThumbnailRenderer", "Generated thumbnail: {}x{} -> {}", 
                  texWidth, texHeight, outputPath.string());
    
    return true;
}

void TextureThumbnailRenderer::GetPreferredSize(u32& outWidth, u32& outHeight) const {
    outWidth = 128;
    outHeight = 128;
}

bool TextureThumbnailRenderer::LoadTexture(const std::filesystem::path& path,
                                           std::vector<u8>& outPixels,
                                           u32& outWidth,
                                           u32& outHeight,
                                           u32& outChannels) {
    // Try to load as .ltex format first
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        LUMA_LOG_ERROR("TextureThumbnailRenderer", "Failed to open file: {}", path.string());
        return false;
    }
    
    // Read header
    LumaTextureHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(LumaTextureHeader));
    
    if (!file.good() || 
        header.magic[0] != 'L' || 
        header.magic[1] != 'T' || 
        header.magic[2] != 'E' || 
        header.magic[3] != 'X') {
        // Not a .ltex file, try stb_image
        file.close();
        
        int w, h, c;
        unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &c, 0);
        
        if (!data) {
            LUMA_LOG_ERROR("TextureThumbnailRenderer", "Failed to load texture with stb_image: {}", path.string());
            return false;
        }
        
        outWidth = static_cast<u32>(w);
        outHeight = static_cast<u32>(h);
        outChannels = static_cast<u32>(c);
        outPixels.assign(data, data + w * h * c);
        
        stbi_image_free(data);
        return true;
    }
    
    // Load .ltex format
    outWidth = header.width;
    outHeight = header.height;
    
    // Determine channels from format
    TextureFormat format = static_cast<TextureFormat>(header.format);
    switch (format) {
        case TextureFormat::R8:
        case TextureFormat::R16:
        case TextureFormat::R16F:
        case TextureFormat::R32F:
            outChannels = 1;
            break;
        case TextureFormat::RG8:
        case TextureFormat::RG16:
        case TextureFormat::RG16F:
        case TextureFormat::RG32F:
            outChannels = 2;
            break;
        case TextureFormat::RGB8:
        case TextureFormat::SRGB8:
        case TextureFormat::RGB16:
        case TextureFormat::RGB16F:
        case TextureFormat::RGB32F:
            outChannels = 3;
            break;
        default:
            outChannels = 4;
            break;
    }
    
    // Read pixel data
    outPixels.resize(header.totalSize);
    file.read(reinterpret_cast<char*>(outPixels.data()), header.totalSize);
    
    file.close();
    
    return true;
}

bool TextureThumbnailRenderer::ResizeTexture(const std::vector<u8>& srcPixels,
                                             u32 srcWidth,
                                             u32 srcHeight,
                                             u32 srcChannels,
                                             std::vector<u8>& dstPixels,
                                             u32 dstWidth,
                                             u32 dstHeight) {
    if (srcWidth == dstWidth && srcHeight == dstHeight) {
        dstPixels = srcPixels;
        return true;
    }
    
    dstPixels.resize(dstWidth * dstHeight * srcChannels);
    
    // Simple bilinear interpolation
    float xRatio = static_cast<float>(srcWidth) / dstWidth;
    float yRatio = static_cast<float>(srcHeight) / dstHeight;
    
    for (u32 y = 0; y < dstHeight; ++y) {
        for (u32 x = 0; x < dstWidth; ++x) {
            float srcX = x * xRatio;
            float srcY = y * yRatio;
            
            u32 x0 = static_cast<u32>(srcX);
            u32 y0 = static_cast<u32>(srcY);
            u32 x1 = std::min(x0 + 1, srcWidth - 1);
            u32 y1 = std::min(y0 + 1, srcHeight - 1);
            
            float fx = srcX - x0;
            float fy = srcY - y0;
            
            for (u32 c = 0; c < srcChannels; ++c) {
                float v00 = srcPixels[(y0 * srcWidth + x0) * srcChannels + c];
                float v10 = srcPixels[(y0 * srcWidth + x1) * srcChannels + c];
                float v01 = srcPixels[(y1 * srcWidth + x0) * srcChannels + c];
                float v11 = srcPixels[(y1 * srcWidth + x1) * srcChannels + c];
                
                float v0 = v00 * (1.0f - fx) + v10 * fx;
                float v1 = v01 * (1.0f - fx) + v11 * fx;
                float v = v0 * (1.0f - fy) + v1 * fy;
                
                dstPixels[(y * dstWidth + x) * srcChannels + c] = static_cast<u8>(std::clamp(v, 0.0f, 255.0f));
            }
        }
    }
    
    return true;
}

void TextureThumbnailRenderer::GenerateCheckerboard(std::vector<u8>& pixels, u32 width, u32 height) {
    const u32 checkerSize = 8;
    
    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            bool light = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;
            u8 color = light ? 240 : 160;
            
            pixels[(y * width + x) * 4 + 0] = color;     // R
            pixels[(y * width + x) * 4 + 1] = color;     // G
            pixels[(y * width + x) * 4 + 2] = color;     // B
            pixels[(y * width + x) * 4 + 3] = 255;       // A
        }
    }
}

bool TextureThumbnailRenderer::ConvertToRGBA8(const std::vector<u8>& srcPixels,
                                               u32 srcWidth,
                                               u32 srcHeight,
                                               u32 srcFormat,
                                               std::vector<u8>& dstPixels) {
    dstPixels.resize(srcWidth * srcHeight * 4);
    
    for (u32 i = 0; i < srcWidth * srcHeight; ++i) {
        u32 srcOffset = i * srcFormat;
        u32 dstOffset = i * 4;
        
        switch (srcFormat) {
            case 1:  // R
                dstPixels[dstOffset + 0] = srcPixels[srcOffset];
                dstPixels[dstOffset + 1] = srcPixels[srcOffset];
                dstPixels[dstOffset + 2] = srcPixels[srcOffset];
                dstPixels[dstOffset + 3] = 255;
                break;
            case 2:  // RG
                dstPixels[dstOffset + 0] = srcPixels[srcOffset + 0];
                dstPixels[dstOffset + 1] = srcPixels[srcOffset + 1];
                dstPixels[dstOffset + 2] = 0;
                dstPixels[dstOffset + 3] = 255;
                break;
            case 3:  // RGB
                dstPixels[dstOffset + 0] = srcPixels[srcOffset + 0];
                dstPixels[dstOffset + 1] = srcPixels[srcOffset + 1];
                dstPixels[dstOffset + 2] = srcPixels[srcOffset + 2];
                dstPixels[dstOffset + 3] = 255;
                break;
            case 4:  // RGBA
                dstPixels[dstOffset + 0] = srcPixels[srcOffset + 0];
                dstPixels[dstOffset + 1] = srcPixels[srcOffset + 1];
                dstPixels[dstOffset + 2] = srcPixels[srcOffset + 2];
                dstPixels[dstOffset + 3] = srcPixels[srcOffset + 3];
                break;
            default:
                return false;
        }
    }
    
    return true;
}

} // namespace Luma