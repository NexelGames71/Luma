#pragma once

#include "Luma/Asset/ThumbnailRenderer.h"
#include "Luma/Asset/LumaTextureFormat.h"

namespace Luma {

/**
 * Thumbnail renderer for texture assets
 * Renders .ltex files as thumbnails
 */
class TextureThumbnailRenderer : public ThumbnailRenderer {
public:
    TextureThumbnailRenderer() = default;
    ~TextureThumbnailRenderer() override = default;
    
    // ThumbnailRenderer interface
    AssetType GetAssetType() const override { return AssetType::Texture; }
    
    bool CanRender(const AssetId& assetId, const std::filesystem::path& nativePath) const override;
    
    bool RenderThumbnail(const AssetId& assetId,
                        const std::filesystem::path& nativePath,
                        const std::filesystem::path& outputPath,
                        u32 width,
                        u32 height) override;
    
    void GetPreferredSize(u32& outWidth, u32& outHeight) const override;
    
    std::string GetRendererName() const override { return "TextureThumbnailRenderer"; }
    
private:
    /**
     * Load texture from .ltex file
     */
    bool LoadTexture(const std::filesystem::path& path,
                    std::vector<u8>& outPixels,
                    u32& outWidth,
                    u32& outHeight,
                    u32& outChannels);
    
    /**
     * Resize texture to thumbnail size
     */
    bool ResizeTexture(const std::vector<u8>& srcPixels,
                      u32 srcWidth,
                      u32 srcHeight,
                      u32 srcChannels,
                      std::vector<u8>& dstPixels,
                      u32 dstWidth,
                      u32 dstHeight);
    
    /**
     * Generate checkerboard pattern for transparent textures
     */
    void GenerateCheckerboard(std::vector<u8>& pixels, u32 width, u32 height);
    
    /**
     * Convert texture format to RGBA8
     */
    bool ConvertToRGBA8(const std::vector<u8>& srcPixels,
                       u32 srcWidth,
                       u32 srcHeight,
                       u32 srcFormat,
                       std::vector<u8>& dstPixels);
};

} // namespace Luma