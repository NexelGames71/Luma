#include "Luma/Asset/ThumbnailRenderer.h"
#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Core/Log.h"

#include <filesystem>
#include <fstream>
#include <chrono>

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
// ThumbnailManager Implementation
// ============================================================================

ThumbnailManager::ThumbnailManager() {
    // Default cache location, used until SetProjectRoot() is called once a
    // project is open; thumbnails then land in <project>/Intermediate/Thumbnails
    // (never inside the project's Assets/ content folder).
    m_cacheDirectory = "Intermediate/Thumbnails";
    
    LUMA_LOG_INFO("ThumbnailManager", "Initialized with cache directory: {}", m_cacheDirectory.string());
}

void ThumbnailManager::Initialize() {
    if (m_isInitialized) {
        return;
    }
    
    // Initialize render type array with default renderers
    InitializeRenderTypeArray();
    
    // Setup checkerboard texture
    SetupCheckerboardTexture();
    
    m_isInitialized = true;
    LUMA_LOG_INFO("ThumbnailManager", "Thumbnail manager initialized");
}

void ThumbnailManager::InitializeRenderTypeArray() {
    // Register default rendering info for texture assets
    ThumbnailRenderingInfo textureInfo;
    textureInfo.assetType = AssetType::Texture;
    textureInfo.rendererName = "TextureThumbnailRenderer";
    textureInfo.useCheckerboard = true;
    textureInfo.defaultWidth = 128;
    textureInfo.defaultHeight = 128;
    m_renderingInfo[AssetType::Texture] = textureInfo;
    
    // Register default rendering info for mesh assets
    ThumbnailRenderingInfo meshInfo;
    meshInfo.assetType = AssetType::Mesh;
    meshInfo.rendererName = "MeshThumbnailRenderer";
    meshInfo.useDefaultPrimitive = false;
    meshInfo.defaultWidth = 128;
    meshInfo.defaultHeight = 128;
    m_renderingInfo[AssetType::Mesh] = meshInfo;

    // Register default rendering info for material assets
    ThumbnailRenderingInfo materialInfo;
    materialInfo.assetType = AssetType::Material;
    materialInfo.rendererName = "MaterialThumbnailRenderer";
    materialInfo.useDefaultPrimitive = true;
    materialInfo.defaultWidth = 128;
    materialInfo.defaultHeight = 128;
    m_renderingInfo[AssetType::Material] = materialInfo;
    
    LUMA_LOG_INFO("ThumbnailManager", "Initialized default rendering info for {} asset types", 
                  m_renderingInfo.size());
}

void ThumbnailManager::RegisterRenderer(AssetType type, std::unique_ptr<ThumbnailRenderer> renderer) {
    if (!renderer) {
        LUMA_LOG_WARN("ThumbnailManager", "Attempted to register null renderer for type {}", static_cast<i32>(type));
        return;
    }
    
    m_renderers[type] = std::move(renderer);
    LUMA_LOG_INFO("ThumbnailManager", "Registered renderer '{}' for asset type {}", 
                  m_renderers[type]->GetRendererName(), static_cast<i32>(type));
}

void ThumbnailManager::UnregisterRenderer(AssetType type) {
    auto it = m_renderers.find(type);
    if (it != m_renderers.end()) {
        LUMA_LOG_INFO("ThumbnailManager", "Unregistered renderer for asset type {}", static_cast<i32>(type));
        m_renderers.erase(it);
    }
}

ThumbnailRenderer* ThumbnailManager::GetRenderer(AssetType type) {
    auto it = m_renderers.find(type);
    if (it != m_renderers.end()) {
        return it->second.get();
    }
    
    LUMA_LOG_WARN("ThumbnailManager", "No renderer registered for asset type {}", static_cast<i32>(type));
    return nullptr;
}

ThumbnailRenderingInfo* ThumbnailManager::GetRenderingInfo(AssetType type) {
    auto it = m_renderingInfo.find(type);
    if (it != m_renderingInfo.end()) {
        return &it->second;
    }
    
    return nullptr;
}

void ThumbnailManager::RegisterRenderingInfo(AssetType type, const ThumbnailRenderingInfo& info) {
    m_renderingInfo[type] = info;
    LUMA_LOG_INFO("ThumbnailManager", "Registered rendering info for asset type {}", static_cast<i32>(type));
}

void ThumbnailManager::SetupCheckerboardTexture() {
    m_checkerboardSize = 32;
    m_checkerboardTexture.resize(m_checkerboardSize * m_checkerboardSize * 4);
    
    const u32 checkerSize = 8;
    for (u32 y = 0; y < m_checkerboardSize; ++y) {
        for (u32 x = 0; x < m_checkerboardSize; ++x) {
            bool light = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;
            u8 color = light ? 240 : 160;
            
            u32 offset = (y * m_checkerboardSize + x) * 4;
            m_checkerboardTexture[offset + 0] = color;     // R
            m_checkerboardTexture[offset + 1] = color;     // G
            m_checkerboardTexture[offset + 2] = color;     // B
            m_checkerboardTexture[offset + 3] = 255;       // A
        }
    }
    
    LUMA_LOG_INFO("ThumbnailManager", "Setup checkerboard texture: {}x{}", m_checkerboardSize, m_checkerboardSize);
}

void ThumbnailManager::DirtyThumbnail(const AssetId& assetId) {
    // Remove the cached thumbnail for this asset
    std::string thumbnailFilename = ToString(assetId) + "_thumb.png";
    auto thumbnailPath = GetCacheDirectory() / thumbnailFilename;
    
    std::error_code ec;
    if (std::filesystem::exists(thumbnailPath, ec)) {
        std::filesystem::remove(thumbnailPath, ec);
        LUMA_LOG_DEBUG("ThumbnailManager", "Dirtied thumbnail for asset: {}", ToString(assetId));
    }
}

u64 ThumbnailManager::GetThumbnailTexture(const AssetId& assetId, const std::filesystem::path& nativePath) {
    // Simplified version - returns 0 for now
    // In production, this would upload the thumbnail to GPU and return a texture handle
    (void)assetId;
    (void)nativePath;
    return 0;
}

void ThumbnailManager::RequestThumbnail(const AssetId& assetId, const std::filesystem::path& nativePath) {
    // Async thumbnail generation - for now, generate synchronously
    ThumbnailSettings settings;
    settings.width = 128;
    settings.height = 128;
    settings.autoRegenerate = true;
    
    std::string thumbnailFilename = ToString(assetId) + "_thumb.png";
    auto thumbnailPath = GetCacheDirectory() / thumbnailFilename;
    
    GenerateThumbnail(assetId, nativePath, thumbnailPath, settings);
}

ThumbnailManager& ThumbnailManager::Get() {
    static ThumbnailManager instance;
    return instance;
}

std::optional<ThumbnailData> ThumbnailManager::GetThumbnail(const AssetId& assetId,
                                                              const std::filesystem::path& nativePath,
                                                              const ThumbnailSettings& settings) {
    // Persistent disk cache first: <project>/Saved/Intermediate/ThumbnailCache
    //
    // This is the fast path the spec calls out — after a project restart we
    // must NOT re-render when a valid cached thumbnail exists. The cache
    // manager validates the sidecar metadata against the asset's current
    // AssetMetadata (source hash + mtime) before returning pixels.
    // Cache manager not available in this version
    // if (ThumbnailCacheManager::Get().IsActive()) {
    //     const AssetMetadata* meta = ResolveMetadata(assetId);
    //     bool metaRequired = (m_registry != nullptr);
    //     if (!metaRequired || meta != nullptr) {
    //         auto cached = ThumbnailCacheManager::Get().LoadFromCache(assetId, meta, settings);
    //         if (cached) {
    //             LUMA_LOG_DEBUG("ThumbnailManager",
    //                            "Disk cache hit for asset {}", ToString(assetId));
    //             return cached;
    //         }
    //     }
    // }

    if (!std::filesystem::exists(nativePath)) {
        LUMA_LOG_WARN("ThumbnailManager", "Asset file does not exist: {}", nativePath.string());
        return std::nullopt;
    }

    std::string thumbnailFilename = ToString(assetId) + "_thumb.png";

    // Thumbnails live exclusively in the project-scoped cache
    // (<project>/Intermediate/Thumbnails) — never inside the content folder
    // beside the source asset. The manager routes m_cacheDirectory there via
    // SetProjectRoot once a project is open.
    std::filesystem::path thumbnailPath =
        m_cacheDirectory / thumbnailFilename;

    // Fast path: a valid, up-to-date cached thumbnail on disk.
    if (std::filesystem::exists(thumbnailPath)) {
        if (!settings.autoRegenerate ||
            !NeedsRegeneration(assetId, nativePath, thumbnailPath)) {
            auto thumbnail = LoadThumbnail(thumbnailPath);
            if (thumbnail && thumbnail->IsValid()) {
                return thumbnail;
            }
        }
    }

    // Generate into the cache directory (created on demand), so the next
    // lookup hits disk.
    bool generated =
        GenerateThumbnail(assetId, nativePath, thumbnailPath, settings);
    if (generated) {
        auto data = LoadThumbnail(thumbnailPath);
        if (data) {
            return data;
        }
    }

    return std::nullopt;
}

const AssetMetadata* ThumbnailManager::ResolveMetadata(const AssetId& id) const {
    // The registry caches its metadata lookup in a local optional. We
    // intentionally return a borrowed pointer only valid for the lifetime
    // of any other outstanding lookup on the same registry; the only
    // caller is GetThumbnail which uses the pointer immediately and never
    // holds it across operations. Returning nullptr when no registry is
    // wired keeps unit tests / cold-start paths alive.
    if (!m_registry) return nullptr;
    return m_registry->GetMetadata(id);
}

void ThumbnailManager::SetProjectRoot(const std::filesystem::path& projectRoot) {
    if (projectRoot.empty()) return;
    // Thumbnails live in the project's scratch area at the project root:
    //   <project>/Intermediate/Thumbnails
    m_cacheDirectory = projectRoot / "Intermediate" / "Thumbnails";
    std::error_code ec;
    std::filesystem::create_directories(m_cacheDirectory, ec);
    if (ec) {
        LUMA_LOG_WARN("ThumbnailManager", "failed to create thumbnail cache dir '{}': {}",
                      m_cacheDirectory.string(), ec.message());
    }
    LUMA_LOG_INFO("ThumbnailManager", "thumbnail cache: {}",
                  m_cacheDirectory.string());
}

void ThumbnailManager::SetRegistry(AssetRegistry* registry) {
    m_registry = registry;
}

// SetRuntimeCache not available in this version

void ThumbnailManager::Invalidate(const AssetId& assetId) {
    // Cache manager not available
    // ThumbnailCacheManager::Get().Invalidate(assetId);
    // if (m_runtimeCache) {
    //     m_runtimeCache->Erase(assetId);
    // }
    (void)assetId;
}

usize ThumbnailManager::CleanupOrphans() {
    if (!m_registry) {
        LUMA_LOG_WARN("ThumbnailManager",
                      "CleanupOrphans called with no registry; nothing to do");
        return 0;
    }
    // Cache manager not available
    // return ThumbnailCacheManager::Get().CleanupOrphans(*m_registry);
    return 0;
}

void ThumbnailManager::ClearAllCache() {
    // Cache manager not available
    // if (m_runtimeCache) {
    //     m_runtimeCache->Clear();
    // }
    // ThumbnailCacheManager::Get().ClearAll();
}

void ThumbnailManager::SetRuntimeCacheLimits(usize maxEntries, usize maxBytes) {
    // Cache manager not available
    (void)maxEntries;
    (void)maxBytes;
}

bool ThumbnailManager::GenerateThumbnail(const AssetId& assetId,
                                         const std::filesystem::path& nativePath,
                                         const std::filesystem::path& outputPath,
                                         const ThumbnailSettings& settings) {
    // Get asset type from file extension
    AssetType type = AssetType::Unknown;
    std::string ext = nativePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".ltex" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".tga" || ext == ".bmp" || ext == ".webp" || ext == ".hdr") {
        type = AssetType::Texture;
    } else if (ext == ".lmesh" || ext == ".fbx" || ext == ".obj" || ext == ".gltf" ||
               ext == ".glb" || ext == ".dae" || ext == ".blend" || ext == ".3ds") {
        type = AssetType::Mesh;
    } else if (ext == ".lmat" || ext == ".lumat" || ext == ".mat") {
        type = AssetType::Material;
    }
    
    if (type == AssetType::Unknown) {
        LUMA_LOG_WARN("ThumbnailManager", "Unknown asset type for file: {} (ext: {})", nativePath.string(), ext);
        return false;
    }
    
    // Get renderer
    ThumbnailRenderer* renderer = GetRenderer(type);
    if (!renderer) {
        LUMA_LOG_ERROR("ThumbnailManager", "No renderer available for asset type {} ({})", 
                       static_cast<i32>(type), nativePath.filename().string());
        return false;
    }
    
    // Verify the file exists
    if (!std::filesystem::exists(nativePath)) {
        LUMA_LOG_ERROR("ThumbnailManager", "Asset file does not exist: {}", nativePath.string());
        return false;
    }
    
    // Ensure cache directory exists
    std::error_code ec;
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec) {
        LUMA_LOG_ERROR("ThumbnailManager", "Failed to create cache directory '{}': {}", 
                       outputPath.parent_path().string(), ec.message());
        return false;
    }
    
    // Render thumbnail
    LUMA_LOG_DEBUG("ThumbnailManager", "Generating thumbnail for {} using {}", 
                   nativePath.filename().string(), renderer->GetRendererName());
    
    bool success = renderer->RenderThumbnail(assetId, nativePath, outputPath, settings.width, settings.height);
    
    if (success) {
        // Verify output file was created
        if (std::filesystem::exists(outputPath, ec)) {
            auto fileSize = std::filesystem::file_size(outputPath, ec);
            LUMA_LOG_INFO("ThumbnailManager", "Thumbnail generated successfully: {} ({} bytes)", 
                          outputPath.filename().string(), fileSize);
        } else {
            LUMA_LOG_ERROR("ThumbnailManager", "Thumbnail file not created after successful generation: {}", 
                           outputPath.string());
            return false;
        }
    } else {
        LUMA_LOG_ERROR("ThumbnailManager", "Failed to generate thumbnail for {} ({}) using {}", 
                       nativePath.filename().string(), ToString(assetId), renderer->GetRendererName());
    }
    
    return success;
}

std::optional<ThumbnailData> ThumbnailManager::LoadThumbnail(const std::filesystem::path& thumbnailPath) {
    if (!std::filesystem::exists(thumbnailPath)) {
        return std::nullopt;
    }
    
    int width, height, channels;
    unsigned char* data = stbi_load(thumbnailPath.string().c_str(), &width, &height, &channels, 4);
    
    if (!data) {
        LUMA_LOG_ERROR("ThumbnailManager", "Failed to load thumbnail: {}", thumbnailPath.string());
        return std::nullopt;
    }
    
    ThumbnailData thumbnail;
    thumbnail.width = static_cast<u32>(width);
    thumbnail.height = static_cast<u32>(height);
    thumbnail.pixels.assign(data, data + width * height * 4);
    
    stbi_image_free(data);
    
    return thumbnail;
}

bool ThumbnailManager::SaveThumbnail(const std::filesystem::path& thumbnailPath, const ThumbnailData& data) {
    if (!data.IsValid()) {
        LUMA_LOG_ERROR("ThumbnailManager", "Attempted to save invalid thumbnail");
        return false;
    }
    
    // Ensure directory exists
    std::error_code ec;
    std::filesystem::create_directories(thumbnailPath.parent_path(), ec);
    
    // Write PNG
    int success = stbi_write_png(thumbnailPath.string().c_str(), 
                                  static_cast<int>(data.width), 
                                  static_cast<int>(data.height), 
                                  4, 
                                  data.pixels.data(), 
                                  static_cast<int>(data.width * 4));
    
    if (!success) {
        LUMA_LOG_ERROR("ThumbnailManager", "Failed to save thumbnail: {}", thumbnailPath.string());
        return false;
    }
    
    return true;
}

bool ThumbnailManager::NeedsRegeneration(const AssetId& assetId,
                                         const std::filesystem::path& nativePath,
                                         const std::filesystem::path& thumbnailPath) {
    if (!std::filesystem::exists(thumbnailPath)) {
        return true;
    }
    
    // Check modification times
    std::error_code ec;
    auto nativeMtime = std::filesystem::last_write_time(nativePath, ec);
    auto thumbMtime = std::filesystem::last_write_time(thumbnailPath, ec);
    
    if (ec) {
        return true;
    }
    
    if (nativeMtime > thumbMtime) {
        return true;
    }
    
    // Check hash
    auto currentHash = AssetMetadataIO::ComputeFileHash(nativePath);
    auto thumbnail = LoadThumbnail(thumbnailPath);
    
    // For now, just rely on modification time comparison
    // Hash comparison would require storing hash separately from PNG
    (void)currentHash;
    (void)thumbnail;
    
    return false;
}

void ThumbnailManager::ClearCache() {
    std::error_code ec;
    if (std::filesystem::exists(m_cacheDirectory, ec)) {
        std::filesystem::remove_all(m_cacheDirectory, ec);
        LUMA_LOG_INFO("ThumbnailManager", "Cleared thumbnail cache");
    }
}

std::filesystem::path ThumbnailManager::GetCacheDirectory() const {
    return m_cacheDirectory;
}

} // namespace Luma