#include "Luma/Asset/AssetId.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace Luma {

namespace {
// FNV-1a 64 constants.
constexpr u64 kFnvOffset = 0xcbf29ce484222325ULL;
constexpr u64 kFnvPrime = 0x100000001b3ULL;

// Maps each character nibble to its hex value.
constexpr char kHex[] = "0123456789abcdef";
}  // namespace

usize HashOf(const AssetId& id) noexcept {
    u64 h = kFnvOffset;
    for (usize i = 0; i < 16; ++i) {
        h ^= static_cast<u64>(id.bytes[i]);
        h *= kFnvPrime;
    }
    return static_cast<usize>(h);
}

std::string ToString(const AssetId& id) {
    // 32 hex chars, no dashes.
    std::string out;
    out.resize(32);
    for (usize i = 0; i < 16; ++i) {
        out[i * 2 + 0] = kHex[(id.bytes[i] >> 4) & 0xF];
        out[i * 2 + 1] = kHex[id.bytes[i] & 0xF];
    }
    return out;
}

AssetId FromString(std::string_view s) {
    AssetId id{};
    if (s.size() < 32) return id;
    for (usize i = 0; i < 16; ++i) {
        auto hi = static_cast<int>(s[i * 2]);
        auto lo = static_cast<int>(s[i * 2 + 1]);
        int v = 0;
        if (hi >= '0' && hi <= '9') v = (hi - '0') << 4;
        else if (hi >= 'a' && hi <= 'f') v = (hi - 'a' + 10) << 4;
        else if (hi >= 'A' && hi <= 'F') v = (hi - 'A' + 10) << 4;
        else return AssetId{};
        if (lo >= '0' && lo <= '9') v |= (lo - '0');
        else if (lo >= 'a' && lo <= 'f') v |= (lo - 'a' + 10);
        else if (lo >= 'A' && lo <= 'F') v |= (lo - 'A' + 10);
        else return AssetId{};
        id.bytes[i] = static_cast<u8>(v & 0xFF);
    }
    return id;
}

AssetId MakeAssetIdFromKey(std::string_view key) {
    // Spread the FNV-1a 64 hash across all 16 bytes with a salt derived
    // from the index. This produces a 128-bit id that is deterministic for
    // a given key but doesn't collapse to 64 bits. Collision-resistance
    // isn't cryptographic — the goal is just stable per-key mapping.
    u64 h = kFnvOffset;
    for (char c : key) {
        h ^= static_cast<u8>(c);
        h *= kFnvPrime;
    }
    AssetId out{};
    u64 mixed = h;
    for (usize i = 0; i < 16; ++i) {
        // Mix the hash with the byte index to spread bits.
        out.bytes[i] = static_cast<u8>((mixed >> ((i % 8) * 8)) ^
                                       ((i * 0x9E) & 0xFF));
        mixed ^= mixed << 13;
        mixed ^= mixed >> 7;
        mixed ^= mixed << 17;
    }
    // Ensure non-zero.
    if (!out.IsValid()) out.bytes[0] = 1;
    return out;
}

}  // namespace Luma
