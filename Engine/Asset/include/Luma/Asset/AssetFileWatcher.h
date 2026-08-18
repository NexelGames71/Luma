#pragma once

#include <filesystem>
#include <memory>
#include <unordered_set>

#include "Luma/Asset/AssetId.h"
#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Asset/AssetImportManager.h"
#include "Luma/VFS/FileWatcher.h"

// Asset File Watcher - bridges the VFS FileWatcher with the Asset Import Manager
// to automatically trigger reimports when source assets are modified.
// Watches source files (not derived files) and queues reimport jobs on change.

namespace Luma {

class AssetFileWatcher {
public:
    AssetFileWatcher();
    ~AssetFileWatcher();
    
    // Initialize with asset registry and import manager
    void Initialize(AssetRegistry* registry, AssetImportManager* importManager);
    
    // Shutdown and stop watching all files
    void Shutdown();
    
    // Watch a specific source file for changes
    void WatchSourceFile(const std::filesystem::path& sourcePath);
    
    // Stop watching a specific file
    void UnwatchSourceFile(const std::filesystem::path& sourcePath);
    
    // Stop watching all files
    void Clear();
    
    // Poll for file changes (call this once per frame from the main thread)
    void Poll();
    
    // Enable/disable automatic reimport (default: enabled)
    void SetAutoReimport(bool enabled) { m_autoReimport = enabled; }
    bool IsAutoReimportEnabled() const { return m_autoReimport; }
    
    // Get number of watched files
    usize WatchedCount() const;
    
private:
    // Callback for file changes
    void OnFileChanged(const VFS::WatchedChange& change);
    
    // Check if a file should be watched (source files only, not derived)
    bool ShouldWatch(const std::filesystem::path& path) const;
    
    // Queue a reimport job for a modified source file
    void QueueReimport(const std::filesystem::path& sourcePath);
    
    AssetRegistry* m_registry = nullptr;
    AssetImportManager* m_importManager = nullptr;
    std::unique_ptr<VFS::FileWatcher> m_fileWatcher;
    
    // Set of files we're currently watching (to avoid duplicates)
    std::unordered_set<std::string> m_watchedPaths;
    
    // Whether to automatically reimport on file changes
    bool m_autoReimport = true;
    
    // Debounce delay in milliseconds to avoid rapid successive reimports
    static constexpr u64 kReimportDebounceMs = 500;
    
    // Track last reimport time per file for debouncing
    std::unordered_map<std::string, u64> m_lastReimportTime;
};

}  // namespace Luma
