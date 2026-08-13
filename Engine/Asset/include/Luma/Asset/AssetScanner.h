#pragma once

#include <filesystem>
#include <functional>
#include <vector>

#include "Luma/Asset/AssetData.h"

// One-shot / one-directory scanner. Walks `root` recursively and emits
// AssetData for every file + folder. Does not own a registry; the caller
// decides what to do with the results. For ongoing / incremental indexing
// use AssetRegistry + a FileWatcher.

namespace Luma {

struct ScanOptions {
    bool includeFolders = true;
    bool followSymlinks = false;
    int maxDepth = -1;  // -1 = unlimited
};

class AssetScanner {
public:
    // Walks `root` once. `opts` controls folders/symlinks/depth.
    static std::vector<AssetData> Scan(const std::filesystem::path& root,
                                       const ScanOptions& opts = {});

    // Same as Scan, but streams each entry to `onEntry` instead of returning
    // a vector. Useful for huge content trees (avoids a big allocation).
    static void ScanStreaming(
        const std::filesystem::path& root,
        const std::function<void(const AssetData&)>& onEntry,
        const ScanOptions& opts = {});

    // True if `path` ends with a dotfile we should ignore (e.g. .DS_Store,
    // .git, .luma_temp). The Content Browser will not show these.
    static bool ShouldIgnore(const std::filesystem::path& path) noexcept;
};

}  // namespace Luma
