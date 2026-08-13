#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Luma/Asset/AssetData.h"
#include "Luma/Asset/AssetId.h"

// In-memory index of every asset under one or more content roots. Owns no
// asset contents, just the descriptors the editor uses to populate the
// Content Browser / filter for the inspector / drive hot-reload.
//
// Concurrency: read-only methods (Size / Lookup / Filter) are safe to call
// concurrently with the *current* snapshot. Mutating calls (Scan /
// AddRoot / Invalidate) take an internal mutex; the public API does not
// pretend to be lock-free — registry mutations happen on the main thread
// (a FileWatcher callback marshaled back to it, typically).

namespace Luma {

class AssetRegistry {
public:
    AssetRegistry() = default;

    // Adds a content root directory (recursive). Files inside are picked up
    // by Scan(); directories themselves become Folder-typed AssetData rows.
    void AddRoot(const std::filesystem::path& root);

    // Returns the set of roots this registry indexes. Order-stable.
    const std::vector<std::filesystem::path>& Roots() const noexcept {
        return m_roots;
    }

    // Returns a user-facing label for `path` rooted at the first registered
    // content root (e.g. "Content/Textures/hero.png"). Falls back to the
    // absolute path string if no root contains it. Used by Content Browser
    // tree rows + breadcrumbs so users never see the system root.
    std::string DisplayPathFor(const std::filesystem::path& path) const;

    // Walks every root and rebuilds the index. Safe to call repeatedly;
    // existing entries are replaced in place so callers holding AssetIds
    // continue to resolve.
    void Scan();

    // Clears all indexed entries. Roots are kept.
    void Clear();

    // Number of assets currently indexed (file + folder rows).
    usize Size() const noexcept;

    // Lookup by AssetId. Returns nullptr if not present.
    const AssetData* Lookup(const AssetId& id) const noexcept;

    // Lookup by absolute package path. Returns nullptr if not present.
    const AssetData* LookupByPath(const std::filesystem::path& path) const noexcept;

    // Returns a snapshot of every asset (file + folder).
    std::vector<const AssetData*> All() const;

    // Returns assets whose type matches. Folder rows are never included.
    std::vector<const AssetData*> FilterByType(AssetType type) const;

    // Returns assets whose packagePath is under the given directory (or any
    // nested directory). Returns empty if dir is not under any root.
    std::vector<const AssetData*> FilterByDirectory(
        const std::filesystem::path& dir) const;

    // Case-insensitive substring filter against the display name. Returns
    // everything if `nameSubstring` is empty. Folder rows are included.
    std::vector<const AssetData*> FilterByName(
        const std::string& nameSubstring) const;

    // Composite filter used by the Content Browser; combines type + directory
    // + name. Either filter may be empty to skip it.
    std::vector<const AssetData*> Filter(
        std::optional<AssetType> type,
        std::optional<std::filesystem::path> dir,
        const std::string& nameSubstring) const;

    // Re-indexes a single path (file or folder). Used by the FileWatcher
    // callback to incrementally update after filesystem changes. No-ops if
    // the path is outside any root.
    void RefreshPath(const std::filesystem::path& path);

    // Drops every entry whose package path is under the given directory.
    // Used when a folder is deleted / moved.
    void RemoveUnder(const std::filesystem::path& dir);

    // Sets the salt used in AssetId derivation. Defaults to "luma". Changing
    // it invalidates all ids; useful for content migrations.
    void SetSalt(std::string s) { m_salt = std::move(s); }
    const std::string& Salt() const noexcept { return m_salt; }

private:
    std::string m_salt = "luma";
    std::vector<std::filesystem::path> m_roots;
    std::unordered_map<AssetId, AssetData> m_byId;
    std::unordered_map<std::string, AssetId> m_byPath;  // path.string() -> id
    mutable std::mutex m_mutex;

    AssetId MakeId(const std::filesystem::path& p) const;
    bool IsUnderAnyRoot(const std::filesystem::path& p) const;
    std::filesystem::path Normalize(const std::filesystem::path& p) const;
};

}  // namespace Luma
