#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Luma/Asset/AssetId.h"
#include "Luma/Asset/AssetType.h"
#include "Luma/Core/Types.h"

// Read-only descriptor for one asset the registry knows about. Owns no
// resources — the asset's content is loaded lazily by other systems.
// `packagePath` is the absolute path on disk (or VFS path); `assetName` is
// the filename without the extension, used for display.

namespace Luma {

struct AssetData {
    AssetId id{};
    AssetType type = AssetType::Unknown;
    std::filesystem::path packagePath;   // absolute, normalized
    std::string assetName;                // filename w/o extension (display)
    std::string extension;                // lowercased, with dot stripped
    i64 mtime = 0;                        // filesystem mtime (Windows: filetime)
    u64 size = 0;                         // bytes
    std::vector<std::string> tags;        // future: package / label tags

    bool IsFolder() const noexcept { return type == AssetType::Folder; }
    bool IsValid() const noexcept { return id.IsValid(); }
};

// Returns the lowercase, dot-stripped extension of a path, or empty.
std::string LowercaseExtension(const std::filesystem::path& p);

}  // namespace Luma
