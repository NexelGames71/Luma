#include "Luma/Asset/AssetFileWatcher.h"

#include <chrono>

#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Asset/AssetImportManager.h"
#include "Luma/Asset/ThumbnailCacheManager.h"
#include "Luma/Asset/ThumbnailRenderer.h"
#include "Luma/Core/Log.h"

namespace Luma {

AssetFileWatcher::AssetFileWatcher() {
    m_fileWatcher = std::make_unique<VFS::FileWatcher>();
}

AssetFileWatcher::~AssetFileWatcher() {
    Shutdown();
}

void AssetFileWatcher::Initialize(AssetRegistry* registry, AssetImportManager* importManager) {
    m_registry = registry;
    m_importManager = importManager;
    LUMA_LOG_INFO("AssetFileWatcher", "Initialized");
}

void AssetFileWatcher::Shutdown() {
    if (m_fileWatcher) {
        m_fileWatcher->Clear();
    }
    m_watchedPaths.clear();
    m_lastReimportTime.clear();
    LUMA_LOG_INFO("AssetFileWatcher", "Shutdown");
}

void AssetFileWatcher::WatchSourceFile(const std::filesystem::path& sourcePath) {
    if (!m_fileWatcher || !ShouldWatch(sourcePath)) {
        return;
    }
    
    std::string pathStr = sourcePath.string();
    
    // Check if already watching
    if (m_watchedPaths.find(pathStr) != m_watchedPaths.end()) {
        return;
    }
    
    // Register callback
    auto callback = [this](const VFS::WatchedChange& change) {
        OnFileChanged(change);
    };
    
    m_fileWatcher->Watch(sourcePath, callback);
    m_watchedPaths.insert(pathStr);
    
    LUMA_LOG_DEBUG("AssetFileWatcher", "Watching: {}", sourcePath.string());
}

void AssetFileWatcher::UnwatchSourceFile(const std::filesystem::path& sourcePath) {
    if (!m_fileWatcher) return;
    
    std::string pathStr = sourcePath.string();
    m_fileWatcher->Unwatch(sourcePath);
    m_watchedPaths.erase(pathStr);
    m_lastReimportTime.erase(pathStr);
    
    LUMA_LOG_DEBUG("AssetFileWatcher", "Unwatching: {}", sourcePath.string());
}

void AssetFileWatcher::Clear() {
    if (m_fileWatcher) {
        m_fileWatcher->Clear();
    }
    m_watchedPaths.clear();
    m_lastReimportTime.clear();
}

void AssetFileWatcher::Poll() {
    if (!m_fileWatcher) return;
    m_fileWatcher->Poll();
}

usize AssetFileWatcher::WatchedCount() const {
    return m_watchedPaths.size();
}

void AssetFileWatcher::OnFileChanged(const VFS::WatchedChange& change) {
    if (!m_autoReimport) return;

    // Deletion handling: drop the registry row AND the persistent cache
    // entry for this asset. Without this the cache would accumulate orphans
    // every time an asset is removed from disk.
    if (change.event == VFS::FileEvent::Deleted) {
        if (!m_registry) return;
        const AssetData* data = m_registry->LookupByPath(change.path);
        if (data && data->id.IsValid()) {
            ThumbnailManager::Get().Invalidate(data->id);
            // Drop the registry row too — RefreshPath sees a missing path
            // and would erase it anyway, but doing it here keeps the
            // browser consistent immediately instead of next-frame.
            m_registry->RefreshPath(change.path);
            UnwatchSourceFile(change.path);
            LUMA_LOG_INFO("AssetFileWatcher",
                          "Asset deleted; cache invalidated: {}",
                          change.path.string());
        }
        return;
    }

    // Renames arrive as Deleted + Created; treat Created as a refresh so
    // the registry picks up the new path under the same GUID. The cache
    // is GUID-keyed, so no invalidation needed.
    if (change.event == VFS::FileEvent::Created) {
        if (!m_registry) return;
        m_registry->RefreshPath(change.path);
        WatchSourceFile(change.path);
        return;
    }

    // Only reimport on modifications.
    if (change.event != VFS::FileEvent::Modified) {
        return;
    }

    // Check if this is a source file we should reimport
    if (!ShouldWatch(change.path)) {
        return;
    }

    QueueReimport(change.path);
}

bool AssetFileWatcher::ShouldWatch(const std::filesystem::path& path) const {
    // Don't watch derived assets (.lmesh, .meta, etc.)
    if (AssetRegistry::IsDerivedAsset(path)) {
        return false;
    }
    
    // Only watch files that have corresponding importers
    if (!m_importManager) return false;
    
    // Check if there's an importer for this file type
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // For now, only watch mesh files
    static const std::vector<std::string> kWatchedExtensions = {
        ".fbx", ".obj", ".gltf", ".glb", ".dae", ".blend"
    };
    
    for (const auto& watched : kWatchedExtensions) {
        if (ext == watched) return true;
    }
    
    return false;
}

void AssetFileWatcher::QueueReimport(const std::filesystem::path& sourcePath) {
    if (!m_importManager || !m_registry) return;
    
    std::string pathStr = sourcePath.string();
    
    // Debounce: check if we recently reimported this file
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    auto it = m_lastReimportTime.find(pathStr);
    if (it != m_lastReimportTime.end()) {
        if (now - it->second < kReimportDebounceMs) {
            LUMA_LOG_DEBUG("AssetFileWatcher", "Debouncing reimport for: {}", sourcePath.string());
            return;
        }
    }
    
    // Update last reimport time
    m_lastReimportTime[pathStr] = now;
    
    // Look up the asset in the registry
    const AssetData* data = m_registry->LookupByPath(sourcePath);
    if (!data || !data->id.IsValid()) {
        LUMA_LOG_WARN("AssetFileWatcher", "Asset not found in registry: {}", sourcePath.string());
        return;
    }
    
    // Check if metadata exists and needs reimport
    auto metaPath = AssetMetadataIO::MetaPathForSource(sourcePath);
    auto meta = AssetMetadataIO::Read(metaPath);
    
    if (meta && !m_importManager->NeedsReimport(*meta)) {
        LUMA_LOG_DEBUG("AssetFileWatcher", "Reimport not needed for: {}", sourcePath.string());
        return;
    }
    
    // Queue reimport job
    LUMA_LOG_INFO("AssetFileWatcher", "Queueing reimport for: {}", sourcePath.string());
    
    m_importManager->QueueReimport(data->id, [](const ImportResult& result) {
        if (result.status == ImportStatus::Completed) {
            LUMA_LOG_INFO("AssetFileWatcher", "Reimport completed successfully");
        } else {
            LUMA_LOG_ERROR("AssetFileWatcher", "Reimport failed: {}", result.errorMessage);
        }
    });
}

}  // namespace Luma
