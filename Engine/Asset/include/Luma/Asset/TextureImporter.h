#pragma once

#include <memory>
#include <string>
#include <filesystem>
#include <vector>

#include "Luma/Asset/Factory.h"
#include "Luma/Asset/AssetImportManager.h"
#include "Luma/Asset/AssetMetadata.h"
#include "Luma/Asset/LumaTextureFormat.h"
#include "Luma/Core/Types.h"

namespace Luma {

// Forward declarations
class AssetRegistry;

/**
 * Texture Importer - imports textures from various image formats
 * Uses stb_image for loading and supports UE5-like texture import options
 */
class TextureImporter : public ReimportFactory {
public:
    TextureImporter();
    ~TextureImporter() override;
    
    // AssetFactory interface
    AssetType GetAssetType() const override { return AssetType::Texture; }
    std::string GetImporterVersion() const { return "1.0.0"; }
    bool CanImport(const std::filesystem::path& sourcePath) const override;
    ImportResult Import(const std::filesystem::path& sourcePath,
                       const std::string& importSettings,
                       const std::filesystem::path& outputDir) override;
    
    // ReimportFactory interface
    std::vector<std::string> GetSupportedExtensions() const override;
    i32 GetPriority() const override { return 90; } // High priority for textures
    std::string GetFactoryName() const override { return "TextureImporter"; }
    std::string GetFactoryVersion() const override { return "1.0.0"; }
    std::string GetDescription() const override { return "Imports textures from PNG, JPG, TGA, BMP, and other image formats"; }
    bool ValidateSource(const std::filesystem::path& sourcePath) const override;
    
    // ReimportFactory interface
    bool NeedsReimport(const AssetId& assetId) const override;
    std::string GetCurrentSettings(const AssetId& assetId) const override;
    ImportResult Reimport(const AssetId& assetId, const std::string& importSettings) override;
    
    // Private helper
    std::string GetDefaultSettings() const;
    void CleanupDerivedFiles(const AssetId& assetId) override;
    std::filesystem::path GetSourcePath(const AssetId& assetId) const override;
    
private:
    // Load texture using stb_image
    struct TextureData {
        i32 width = 0;
        i32 height = 0;
        i32 channels = 0;
        std::vector<u8> pixels;
    };
    
    std::optional<TextureData> LoadTexture(const std::filesystem::path& sourcePath);
    
    // Process texture according to import settings
    bool ProcessTexture(TextureData& texture, const TextureImportSettings& settings);
    
    // Generate mipmaps
    bool GenerateMipmaps(TextureData& texture);
    
    // Resize texture
    bool ResizeTexture(TextureData& texture, i32 newWidth, i32 newHeight);
    
    // Convert color space
    bool ConvertToSRGB(TextureData& texture);
    
    // Flip texture vertically
    bool FlipTexture(TextureData& texture);
    
    // Pack channels
    bool PackChannels(TextureData& texture, const TextureImportSettings& settings);
    
    // Apply alpha threshold
    bool ApplyAlphaThreshold(TextureData& texture, i32 threshold);
    
    // Save processed texture to native Luma format
    bool SaveLumaTexture(const TextureData& texture, 
                        const std::filesystem::path& outputPath,
                        const TextureImportSettings& settings);
    
    // Update metadata after successful import
    bool UpdateMetadata(const std::filesystem::path& sourcePath,
                      const std::filesystem::path& nativePath,
                      const TextureImportSettings& settings,
                      const std::string& importerVersion);
    
    // Asset registry pointer for reimport operations
    AssetRegistry* m_registry = nullptr;
    
    // Set the asset registry (called by import manager)
    void SetRegistry(AssetRegistry* registry) { m_registry = registry; }
};

} // namespace Luma