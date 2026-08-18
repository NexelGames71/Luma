#include "Luma/Asset/TextureImporter.h"
#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Asset/AssetMetadata.h"
#include "Luma/Asset/LumaTextureFormat.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4505)
#endif
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "Luma/Core/Log.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <chrono>

namespace Luma {

// ============================================================================
// TextureImporter Implementation
// ============================================================================

TextureImporter::TextureImporter() {
    LUMA_LOG_INFO("TextureImporter", "Initialized");
}

TextureImporter::~TextureImporter() {
    LUMA_LOG_INFO("TextureImporter", "Shutdown");
}

// ============================================================================
// Private Helper Functions
// ============================================================================

std::optional<TextureImporter::TextureData> TextureImporter::LoadTexture(
    const std::filesystem::path& sourcePath) {
    
    TextureData data;
    int w, h, c;
    stbi_uc* rawPixels = stbi_load(sourcePath.string().c_str(), &w, &h, &c, 0);
    
    if (!rawPixels) {
        LUMA_LOG_ERROR("TextureImporter", "Failed to load texture: {}", sourcePath.string());
        return std::nullopt;
    }
    
    data.width = w;
    data.height = h;
    data.channels = c;
    
    // Copy raw pixels into vector
    size_t pixelCount = static_cast<size_t>(w * h * c);
    data.pixels.assign(rawPixels, rawPixels + pixelCount);
    
    // Free the raw data
    stbi_image_free(rawPixels);
    
    LUMA_LOG_INFO("TextureImporter", "Loaded texture: {}x{}, {} channels", 
                  data.width, data.height, data.channels);
    
    return data;
}

bool TextureImporter::ProcessTexture(TextureData& texture, const TextureImportSettings& settings) {
    // Apply flip if requested
    if (settings.flipY) {
        if (!FlipTexture(texture)) {
            return false;
        }
    }
    
    // Apply scaling if needed
    if (settings.scale != 1.0f) {
        i32 newWidth = static_cast<i32>(texture.width * settings.scale);
        i32 newHeight = static_cast<i32>(texture.height * settings.scale);
        if (!ResizeTexture(texture, newWidth, newHeight)) {
            return false;
        }
    }
    
    // Apply desired size if specified
    if (settings.desiredSize > 0) {
        if (!ResizeTexture(texture, settings.desiredSize, settings.desiredSize)) {
            return false;
        }
    }
    
    // Enforce maximum size
    if (texture.width > settings.maxTextureSize || texture.height > settings.maxTextureSize) {
        f32 aspect = static_cast<f32>(texture.width) / static_cast<f32>(texture.height);
        i32 newWidth, newHeight;
        
        if (texture.width > texture.height) {
            newWidth = settings.maxTextureSize;
            newHeight = static_cast<i32>(settings.maxTextureSize / aspect);
        } else {
            newHeight = settings.maxTextureSize;
            newWidth = static_cast<i32>(settings.maxTextureSize * aspect);
        }
        
        if (!ResizeTexture(texture, newWidth, newHeight)) {
            return false;
        }
    }
    
    // Apply alpha threshold if set
    if (settings.alphaThreshold > 0 && texture.channels >= 4) {
        if (!ApplyAlphaThreshold(texture, settings.alphaThreshold)) {
            return false;
        }
    }
    
    // Convert to sRGB if requested
    if (settings.sRGB) {
        if (!ConvertToSRGB(texture)) {
            return false;
        }
    }
    
    // Generate mipmaps if requested
    if (settings.generateMipmaps) {
        if (!GenerateMipmaps(texture)) {
            return false;
        }
    }
    
    return true;
}

bool TextureImporter::GenerateMipmaps(TextureData& texture) {
    // Placeholder for mipmap generation
    // In production, implement proper mipmap generation
    LUMA_LOG_DEBUG("TextureImporter", "Mipmap generation not yet implemented");
    return true;
}

bool TextureImporter::ResizeTexture(TextureData& texture, i32 newWidth, i32 newHeight) {
    if (newWidth == texture.width && newHeight == texture.height) {
        return true;
    }
    
    // Simple nearest-neighbor resize (in production, use proper image scaling)
    std::vector<u8> newPixels(newWidth * newHeight * texture.channels);
    
    f32 xRatio = static_cast<f32>(texture.width) / newWidth;
    f32 yRatio = static_cast<f32>(texture.height) / newHeight;
    
    for (i32 y = 0; y < newHeight; ++y) {
        for (i32 x = 0; x < newWidth; ++x) {
            i32 srcX = static_cast<i32>(x * xRatio);
            i32 srcY = static_cast<i32>(y * yRatio);
            
            for (i32 c = 0; c < texture.channels; ++c) {
                newPixels[(y * newWidth + x) * texture.channels + c] = 
                    texture.pixels[(srcY * texture.width + srcX) * texture.channels + c];
            }
        }
    }
    
    texture.pixels = newPixels;
    texture.width = newWidth;
    texture.height = newHeight;
    
    LUMA_LOG_INFO("TextureImporter", "Resized texture to {}x{}", newWidth, newHeight);
    return true;
}

bool TextureImporter::ConvertToSRGB(TextureData& texture) {
    // Simple linear to sRGB conversion
    auto linearToSRGB = [](f32 linear) -> f32 {
        if (linear <= 0.0031308f) {
            return linear * 12.92f;
        }
        return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
    };
    
    for (size_t i = 0; i < texture.pixels.size(); ++i) {
        // Only convert RGB channels, not alpha
        i32 channel = i % texture.channels;
        if (channel < 3) { // RGB channels
            f32 linear = texture.pixels[i] / 255.0f;
            f32 srgb = linearToSRGB(linear);
            texture.pixels[i] = static_cast<u8>(std::clamp(srgb * 255.0f, 0.0f, 255.0f));
        }
    }
    
    return true;
}

bool TextureImporter::FlipTexture(TextureData& texture) {
    i32 rowSize = texture.width * texture.channels;
    std::vector<u8> tempRow(rowSize);
    
    for (i32 y = 0; y < texture.height / 2; ++y) {
        i32 srcRow = y * rowSize;
        i32 dstRow = (texture.height - 1 - y) * rowSize;
        
        std::memcpy(tempRow.data(), &texture.pixels[srcRow], rowSize);
        std::memcpy(&texture.pixels[srcRow], &texture.pixels[dstRow], rowSize);
        std::memcpy(&texture.pixels[dstRow], tempRow.data(), rowSize);
    }
    
    return true;
}

bool TextureImporter::PackChannels(TextureData& texture, const TextureImportSettings& settings) {
    (void)settings;
    // Placeholder for channel packing
    LUMA_LOG_DEBUG("TextureImporter", "Channel packing not yet implemented");
    return true;
}

bool TextureImporter::ApplyAlphaThreshold(TextureData& texture, i32 threshold) {
    for (size_t i = 3; i < texture.pixels.size(); i += texture.channels) {
        if (texture.pixels[i] < threshold) {
            texture.pixels[i] = 0;
        } else {
            texture.pixels[i] = 255;
        }
    }
    
    return true;
}

bool TextureImporter::SaveLumaTexture(const TextureData& texture,
                                     const std::filesystem::path& outputPath,
                                     const TextureImportSettings& settings) {
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        LUMA_LOG_ERROR("TextureImporter", "Failed to open output file: {}", outputPath.string());
        return false;
    }
    
    // Determine texture format
    TextureFormat format;
    if (settings.normalMap) {
        format = texture.channels == 4 ? TextureFormat::RGBA8 : TextureFormat::RGB8;
    } else if (settings.sRGB) {
        format = texture.channels == 4 ? TextureFormat::SRGBA8 : TextureFormat::SRGB8;
    } else {
        format = texture.channels == 4 ? TextureFormat::RGBA8 : TextureFormat::RGB8;
    }
    
    // Write header
    LumaTextureHeader header;
    header.width = texture.width;
    header.height = texture.height;
    header.format = static_cast<u32>(format);
    header.dataType = static_cast<u32>(TextureDataType::UNorm);
    header.mipLevels = 1; // TODO: Support mipmaps
    header.totalSize = static_cast<u32>(texture.pixels.size());
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(LumaTextureHeader));
    
    // Write pixel data
    file.write(reinterpret_cast<const char*>(texture.pixels.data()), texture.pixels.size());
    
    file.close();
    
    LUMA_LOG_INFO("TextureImporter", "Saved Luma texture to: {}", outputPath.string());
    return true;
}

bool TextureImporter::UpdateMetadata(const std::filesystem::path& sourcePath,
                                   const std::filesystem::path& nativePath,
                                   const TextureImportSettings& settings,
                                   const std::string& importerVersion) {
    AssetMetadata meta;
    meta.guid = MakeAssetIdFromKey(sourcePath.string());
    meta.type = AssetType::Texture;
    meta.sourcePath = sourcePath;
    meta.nativePath = nativePath;
    meta.importerVersion = importerVersion;
    meta.importSettings = settings.ToJson();
    meta.isImporting = false;
    meta.lastError.clear();
    
    // Get current file metadata
    std::error_code ec;
    meta.sourceMtime = std::chrono::duration_cast<std::chrono::seconds>(
        std::filesystem::last_write_time(sourcePath, ec).time_since_epoch()).count();
    meta.sourceHash = AssetMetadataIO::ComputeFileHash(sourcePath);
    meta.importTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Write metadata file
    auto metaPath = AssetMetadataIO::MetaPathForSource(sourcePath);
    if (!AssetMetadataIO::Write(metaPath, meta)) {
        LUMA_LOG_ERROR("TextureImporter", "Failed to write metadata file: {}", metaPath.string());
        return false;
    }
    
    // Update registry if available
    if (m_registry) {
        m_registry->SaveMetadata(meta.guid, meta);
    }
    
    return true;
}

std::string TextureImporter::GetDefaultSettings() const {
    return TextureImportSettings::GetDefaults().ToJson();
}

// ============================================================================
// AssetFactory Interface Implementation
// ============================================================================

std::vector<std::string> TextureImporter::GetSupportedExtensions() const {
    return {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".psd", ".gif", ".hdr", ".pic"};
}

bool TextureImporter::CanImport(const std::filesystem::path& sourcePath) const {
    if (!std::filesystem::exists(sourcePath)) {
        return false;
    }
    return HasSupportedExtension(sourcePath);
}

ImportResult TextureImporter::Import(const std::filesystem::path& sourcePath,
                                     const std::string& importSettings,
                                     const std::filesystem::path& outputDir) {
    ImportResult result;
    result.status = ImportStatus::Failed;
    result.sourcePath = sourcePath;
    
    LUMA_LOG_INFO("TextureImporter", "Importing texture: {}", sourcePath.string());
    
    // Parse import settings
    TextureImportSettings settings = TextureImportSettings::GetDefaults();
    if (!importSettings.empty()) {
        settings = TextureImportSettings::FromJson(importSettings);
    }
    
    // Load source texture
    auto textureData = LoadTexture(sourcePath);
    if (!textureData) {
        result.errorMessage = "Failed to load texture file";
        return result;
    }
    
    // Process texture according to settings
    if (!ProcessTexture(*textureData, settings)) {
        result.errorMessage = "Failed to process texture";
        return result;
    }
    
    // Generate output path
    std::string outputFilename = sourcePath.stem().string() + ".ltex";
    auto outputPath = outputDir / outputFilename;
    
    // Save processed texture in Luma format
    if (!SaveLumaTexture(*textureData, outputPath, settings)) {
        result.errorMessage = "Failed to save texture";
        return result;
    }
    
    // Update metadata
    if (!UpdateMetadata(sourcePath, outputPath, settings, GetImporterVersion())) {
        result.errorMessage = "Failed to update metadata";
        return result;
    }
    
    result.status = ImportStatus::Completed;
    result.nativePath = outputPath;
    result.assetId = MakeAssetIdFromKey(sourcePath.string());
    
    LUMA_LOG_INFO("TextureImporter", "Successfully imported texture: {}", sourcePath.string());
    return result;
}

bool TextureImporter::ValidateSource(const std::filesystem::path& sourcePath) const {
    if (!AssetFactory::ValidateSource(sourcePath)) {
        return false;
    }
    
    // Try to load with stb_image to validate it's a valid image file
    i32 width, height, channels;
    unsigned char* data = stbi_load(sourcePath.string().c_str(), &width, &height, &channels, 0);
    
    if (!data) {
        return false;
    }
    
    stbi_image_free(data);
    
    // Check minimum size
    if (width < 1 || height < 1) {
        return false;
    }
    
    // Check maximum size
    if (width > 16384 || height > 16384) {
        return false;
    }
    
    return true;
}

// ============================================================================
// ReimportFactory Interface Implementation
// ============================================================================

bool TextureImporter::NeedsReimport(const AssetId& assetId) const {
    if (!m_registry) {
        return false;
    }
    
    auto meta = m_registry->GetMetadata(assetId);
    if (!meta) {
        return false;
    }
    
    if (!std::filesystem::exists(meta->sourcePath)) {
        return false;
    }
    
    std::error_code ec;
    auto currentMtime = std::chrono::duration_cast<std::chrono::seconds>(
        std::filesystem::last_write_time(meta->sourcePath, ec).time_since_epoch()).count();
    
    if (currentMtime != meta->sourceMtime) {
        return true;
    }
    
    auto currentHash = AssetMetadataIO::ComputeFileHash(meta->sourcePath);
    if (currentHash != meta->sourceHash) {
        return true;
    }
    
    if (GetImporterVersion() != meta->importerVersion) {
        return true;
    }
    
    return false;
}

std::string TextureImporter::GetCurrentSettings(const AssetId& assetId) const {
    if (!m_registry) {
        return GetDefaultSettings();
    }
    
    auto meta = m_registry->GetMetadata(assetId);
    if (!meta) {
        return GetDefaultSettings();
    }
    
    return meta->importSettings;
}

ImportResult TextureImporter::Reimport(const AssetId& assetId, const std::string& importSettings) {
    ImportResult result;
    result.status = ImportStatus::Failed;
    result.assetId = assetId;
    
    if (!m_registry) {
        result.errorMessage = "Asset registry not set";
        return result;
    }
    
    auto meta = m_registry->GetMetadata(assetId);
    if (!meta) {
        result.errorMessage = "Asset metadata not found";
        return result;
    }
    
    if (!std::filesystem::exists(meta->sourcePath)) {
        result.errorMessage = "Source file not found: " + meta->sourcePath.string();
        return result;
    }
    
    std::string settingsToUse = importSettings.empty() ? meta->importSettings : importSettings;
    auto outputDir = meta->nativePath.parent_path();
    
    auto importResult = Import(meta->sourcePath, settingsToUse, outputDir);
    importResult.assetId = assetId;
    
    return importResult;
}

void TextureImporter::CleanupDerivedFiles(const AssetId& assetId) {
    if (!m_registry) {
        return;
    }
    
    auto meta = m_registry->GetMetadata(assetId);
    if (!meta) {
        return;
    }
    
    std::error_code ec;
    
    if (std::filesystem::exists(meta->nativePath, ec)) {
        std::filesystem::remove(meta->nativePath, ec);
    }
    
    auto metaPath = AssetMetadataIO::MetaPathForSource(meta->sourcePath);
    if (std::filesystem::exists(metaPath, ec)) {
        std::filesystem::remove(metaPath, ec);
    }
}

std::filesystem::path TextureImporter::GetSourcePath(const AssetId& assetId) const {
    if (!m_registry) return {};
    auto meta = m_registry->GetMetadata(assetId);
    if (meta && !meta->sourcePath.empty()) {
        return meta->sourcePath;
    }
    return {};
}

} // namespace Luma