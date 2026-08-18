#pragma once

#include "Luma/Core/Types.h"
#include "Luma/Asset/AssetId.h"
#include "Luma/Asset/AssetType.h"
#include "Luma/Asset/AssetMetadata.h"
#include "Luma/Asset/ThumbnailRenderingInfo.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Luma {

// Forward declarations
class AssetRegistry;

/**
 * Thumbnail renderer base class
 * Each asset type implements its own renderer to generate thumbnails
 */
class ThumbnailRenderer {
public:
    virtual ~ThumbnailRenderer() = default;
    
    /**
     * Get the asset type this renderer handles
     */
    virtual AssetType GetAssetType() const = 0;
    
    /**
     * Check if this renderer can generate a thumbnail for the given asset
     */
    virtual bool CanRender(const AssetId& assetId, const std::filesystem::path& nativePath) const = 0;
    
    /**
     * Generate a thumbnail for the asset
     * @param assetId The asset ID
     * @param nativePath Path to the native asset file
     * @param outputPath Where to save the thumbnail image
     * @param width Thumbnail width
     * @param height Thumbnail height
     * @return true if thumbnail was generated successfully
     */
    virtual bool RenderThumbnail(const AssetId& assetId,
                                const std::filesystem::path& nativePath,
                                const std::filesystem::path& outputPath,
                                u32 width,
                                u32 height) = 0;
    
    /**
     * Get preferred thumbnail size for this asset type
     */
    virtual void GetPreferredSize(u32& outWidth, u32& outHeight) const {
        outWidth = 128;
        outHeight = 128;
    }
    
    /**
     * Get renderer name for debugging
     */
    virtual std::string GetRendererName() const = 0;
};

/**
 * Thumbnail settings for an asset
 */
struct ThumbnailSettings {
    u32 width = 128;
    u32 height = 128;
    bool autoRegenerate = true;
    bool useCheckerboard = true;  // For transparent textures
    bool showOverlays = true;     // Show asset type overlays
};

/**
 * Thumbnail data structure
 */
struct ThumbnailData {
    std::vector<u8> pixels;  // RGBA8 format
    u32 width = 0;
    u32 height = 0;
    std::string sourceHashStr;       // Hash of source asset to detect changes
    i64 lastModified = 0;     // Timestamp when thumbnail was generated
    
    bool IsValid() const {
        return !pixels.empty() && width > 0 && height > 0;
    }
};

/**
 * Thumbnail manager - handles thumbnail generation and caching
 * Based on Unreal Engine's UThumbnailManager architecture
 */
class ThumbnailManager {
public:
    static ThumbnailManager& Get();
    
    /**
     * Initialize the thumbnail manager and load default resources
     */
    void Initialize();
    
    /**
     * Register a thumbnail renderer for an asset type
     */
    void RegisterRenderer(AssetType type, std::unique_ptr<ThumbnailRenderer> renderer);
    
    /**
     * Unregister a thumbnail renderer for an asset type
     */
    void UnregisterRenderer(AssetType type);
    
    /**
     * Get the renderer for an asset type
     */
    ThumbnailRenderer* GetRenderer(AssetType type);
    
    /**
     * Get rendering info for an asset type
     */
    ThumbnailRenderingInfo* GetRenderingInfo(AssetType type);
    
    /**
     * Register rendering info for an asset type
     */
    void RegisterRenderingInfo(AssetType type, const ThumbnailRenderingInfo& info);
    
    /**
     * Generate or retrieve a thumbnail for an asset
     * @param assetId The asset ID
     * @param nativePath Path to the native asset file
     * @param settings Thumbnail generation settings
     * @return Thumbnail data, or nullopt if generation failed
     */
    std::optional<ThumbnailData> GetThumbnail(const AssetId& assetId,
                                               const std::filesystem::path& nativePath,
                                               const ThumbnailSettings& settings = {});
    
    /**
     * Generate a thumbnail and save it to disk
     * @param assetId The asset ID
     * @param nativePath Path to the native asset file
     * @param outputPath Where to save the thumbnail
     * @param settings Thumbnail generation settings
     * @return true if thumbnail was generated and saved successfully
     */
    bool GenerateThumbnail(const AssetId& assetId,
                          const std::filesystem::path& nativePath,
                          const std::filesystem::path& outputPath,
                          const ThumbnailSettings& settings = {});
    
    /**
     * Load a thumbnail from disk
     */
    std::optional<ThumbnailData> LoadThumbnail(const std::filesystem::path& thumbnailPath);
    
    /**
     * Save a thumbnail to disk
     */
    bool SaveThumbnail(const std::filesystem::path& thumbnailPath, const ThumbnailData& data);
    
    /**
     * Check if a thumbnail needs to be regenerated
     */
    bool NeedsRegeneration(const AssetId& assetId,
                           const std::filesystem::path& nativePath,
                           const std::filesystem::path& thumbnailPath);
    
    /**
     * Clear the thumbnail cache
     */
    void ClearCache();
    
    /**
     * Dirty a thumbnail (mark it for regeneration)
     */
    void DirtyThumbnail(const AssetId& assetId);
    
    /**
     * Get the thumbnail cache directory
     */
    std::filesystem::path GetCacheDirectory() const;

    /**
     * Set the project root. When set, the manager routes its persistent
     * thumbnail cache to
     *   <project>/Intermediate/Thumbnails
     * (the project's scratch folder at the project root), instead of the
     * default "Intermediate/Thumbnails" working-directory path. Called by
     * EditorScreen once the project is loaded. Safe to call multiple times.
     */
    void SetProjectRoot(const std::filesystem::path& projectRoot);

    /**
     * Provide a registry so GetThumbnail can validate cached entries against
     * each asset's current AssetMetadata (the source hash / mtime pair).
     * Without a registry the cache falls back to "valid if files exist" —
     * acceptable before project load, never acceptable once the editor is up.
     */
    void SetRegistry(AssetRegistry* registry);

    /**
     * Install a runtime LRU cache for already-uploaded GPU textures.
     * Cache manager not available in this version.
     */
    // void SetRuntimeCache(ThumbnailLRUCache* cache,
    //                      void (*onEvicted)(u64 handle, void* userData),
    //                      void* userData);

    /**
     * Invalidate one asset's cache entry (both persistent disk + LRU). Use
     * when an asset is reimported, deleted, or otherwise changed. Idempotent.
     */
    void Invalidate(const AssetId& assetId);

    /**
     * Delete every entry in the persistent cache whose asset is no longer in
     * the registry. Called periodically or as an explicit maintenance action.
     * Returns the number of entries removed. Requires SetRegistry() first.
     */
    usize CleanupOrphans();

    /**
     * Hard reset: drop both the persistent disk cache and the LRU runtime
     * cache. Recreates the disk directory empty so subsequent lookups start
     * fresh. Intended for the explicit "Clear Thumbnail Cache" action.
     */
    void ClearAllCache();

    /**
     * Configure bounds on the LRU when the manager owns it. No-op if a
     * runtime cache was provided externally. Values <= 0 disable a given
     * limit.
     */
    void SetRuntimeCacheLimits(usize maxEntries, usize maxBytes);

    /**
     * Setup checkerboard texture for transparent thumbnails
     */
    void SetupCheckerboardTexture();
    
    /**
     * Get the checkerboard texture data
     */
    const std::vector<u8>& GetCheckerboardTexture() const { return m_checkerboardTexture; }
    
    /**
     * Get thumbnail texture handle for an asset (simplified version)
     * Returns 0 if thumbnail doesn't exist or failed to load
     */
    u64 GetThumbnailTexture(const AssetId& assetId, const std::filesystem::path& nativePath);
    
    /**
     * Request thumbnail generation (async-friendly version)
     */
    void RequestThumbnail(const AssetId& assetId, const std::filesystem::path& nativePath);
    
private:
    ThumbnailManager();
    ~ThumbnailManager() = default;

    void InitializeRenderTypeArray();

    // Resolve the AssetMetadata for `id` through the registry if available.
    // Returns nullptr when the registry is unset or the asset isn't indexed.
    // Used by GetThumbnail to validate the persistent cache key against the
    // source asset's current hash/mtime.
    const AssetMetadata* ResolveMetadata(const AssetId& id) const;

    std::unordered_map<AssetType, std::unique_ptr<ThumbnailRenderer>> m_renderers;
    std::unordered_map<AssetType, ThumbnailRenderingInfo> m_renderingInfo;
    std::filesystem::path m_cacheDirectory;
    bool m_isInitialized = false;

    // Optional references; never owned by the manager.
    AssetRegistry* m_registry = nullptr;
    // ThumbnailLRUCache* m_runtimeCache = nullptr;
    // void (*m_runtimeEvictCb)(u64, void*) = nullptr;
    // void* m_runtimeEvictUserData = nullptr;

    // Fallback in-memory LRU used when nobody installed one externally.
    // std::unique_ptr<ThumbnailLRUCache> m_ownedRuntimeCache;

    // Checkerboard texture for transparent thumbnails
    std::vector<u8> m_checkerboardTexture;
    u32 m_checkerboardSize = 32;
};

} // namespace Luma