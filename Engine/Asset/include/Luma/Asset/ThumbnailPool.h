#pragma once

#include "Luma/Core/Types.h"
#include "Luma/Asset/AssetId.h"
#include <vector>
#include <memory>

namespace Luma {

// Forward declarations
class ThumbnailRenderer;

/**
 * Thumbnail pool for efficient thumbnail rendering
 * Based on Unreal's FAssetThumbnailPool
 */
class ThumbnailPool {
public:
    ThumbnailPool(u32 poolSize = 32);
    ~ThumbnailPool();
    
    /**
     * Request a thumbnail for an asset
     * Returns true if thumbnail was generated or loaded from cache
     */
    bool RequestThumbnail(const AssetId& assetId, const std::filesystem::path& nativePath);
    
    /**
     * Get the thumbnail texture handle for an asset
     */
    u64 GetThumbnailTexture(const AssetId& assetId) const;
    
    /**
     * Clear all cached thumbnails
     */
    void Clear();
    
    /**
     * Update the pool (process pending requests)
     */
    void Update();
    
private:
    struct ThumbnailEntry {
        AssetId assetId;
        std::filesystem::path nativePath;
        u64 textureHandle = 0;
        bool requested = false;
        bool loaded = false;
    };
    
    std::vector<ThumbnailEntry> m_entries;
    u32 m_poolSize;
    u32 m_currentIndex = 0;
};

} // namespace Luma