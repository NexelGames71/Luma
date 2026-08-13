#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>

#include "Luma/Core/Types.h"

// 128-bit content-addressed identifier for an asset. Two AssetIds are equal
// iff their bytes are equal. IDs are derived from a per-asset UUID (currently
// a SHA1-style hash of the absolute path + a salt; deterministic on every
// machine so team-shared content indexes match). Stable across renames if
// the underlying file content hash is reused, but path-based IDs are simpler
// and stable enough for an editor's runtime indexing.
//
// API uses small-inline storage (16 bytes; no heap). Hashing uses the FNV-1a
// 64 mix over the 16 bytes for use as an unordered_map key.

namespace Luma {

struct AssetId {
    u8 bytes[16]{};

    constexpr AssetId() noexcept = default;

    bool IsValid() const noexcept {
        // Non-zero after construction is the convention for "valid".
        for (usize i = 0; i < 16; ++i)
            if (bytes[i] != 0) return true;
        return false;
    }

    bool operator==(const AssetId& other) const noexcept {
        return std::memcmp(bytes, other.bytes, 16) == 0;
    }
    bool operator!=(const AssetId& other) const noexcept {
        return !(*this == other);
    }
    bool operator<(const AssetId& other) const noexcept {
        return std::memcmp(bytes, other.bytes, 16) < 0;
    }
};

// FNV-1a 64-bit over the raw bytes — used as unordered_map key.
usize HashOf(const AssetId& id) noexcept;

std::string ToString(const AssetId& id);
AssetId FromString(std::string_view s);  // returns invalid id if malformed

// Synthesizes a deterministic AssetId from a stable string key (e.g. the
// absolute file path). Two calls with the same key on any machine return
// equal ids.
AssetId MakeAssetIdFromKey(std::string_view key);

}  // namespace Luma

// std::hash specialization so AssetId works as an unordered_map / set key.
namespace std {
template <>
struct hash<Luma::AssetId> {
    size_t operator()(const Luma::AssetId& id) const noexcept {
        return static_cast<size_t>(Luma::HashOf(id));
    }
};
}  // namespace std
