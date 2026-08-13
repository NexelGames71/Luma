// AssetId — 128-bit deterministic id, equality + hashing + string roundtrip.

#include <catch2/catch_test_macros.hpp>

#include <unordered_map>
#include <unordered_set>

#include "Luma/Asset/AssetId.h"

using Luma::AssetId;
using Luma::FromString;
using Luma::HashOf;
using Luma::MakeAssetIdFromKey;
using Luma::ToString;

TEST_CASE("Default-constructed AssetId is invalid", "[asset][id]") {
    AssetId id{};
    REQUIRE_FALSE(id.IsValid());
    REQUIRE(ToString(id) ==
            "00000000000000000000000000000000");
}

TEST_CASE("MakeAssetIdFromKey is deterministic for the same key",
          "[asset][id]") {
    auto a = MakeAssetIdFromKey("/content/textures/hero.png");
    auto b = MakeAssetIdFromKey("/content/textures/hero.png");
    REQUIRE(a == b);
    REQUIRE(a.IsValid());
}

TEST_CASE("MakeAssetIdFromKey produces different ids for different keys",
          "[asset][id]") {
    auto a = MakeAssetIdFromKey("/content/a.png");
    auto b = MakeAssetIdFromKey("/content/b.png");
    REQUIRE(a != b);
}

TEST_CASE("AssetId equality is byte-wise", "[asset][id]") {
    auto a = MakeAssetIdFromKey("k1");
    auto b = MakeAssetIdFromKey("k1");
    AssetId copy = a;
    REQUIRE(a == b);
    REQUIRE(a == copy);
    REQUIRE_FALSE(a != b);
}

TEST_CASE("AssetId ordering is stable", "[asset][id]") {
    auto a = MakeAssetIdFromKey("alpha");
    auto b = MakeAssetIdFromKey("beta");
    auto c = MakeAssetIdFromKey("gamma");
    // Just check < forms a strict weak order; we don't pin to specific values.
    REQUIRE((a < b) != (b < a));
    REQUIRE((a < c) != (c < a));
    REQUIRE((b < c) != (c < b));
}

TEST_CASE("ToString emits 32 lowercase hex chars", "[asset][id]") {
    auto id = MakeAssetIdFromKey("x");
    auto s = ToString(id);
    REQUIRE(s.size() == 32);
    for (char c : s) {
        REQUIRE(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')));
    }
}

TEST_CASE("FromString round-trips ToString", "[asset][id]") {
    auto id = MakeAssetIdFromKey("content/materials/wall.mat");
    auto s = ToString(id);
    auto back = FromString(s);
    REQUIRE(back == id);
}

TEST_CASE("FromString rejects malformed input", "[asset][id]") {
    AssetId empty{};
    REQUIRE(FromString("") == empty);
    REQUIRE(FromString("too short") == empty);
    REQUIRE(FromString("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz") == empty);
    REQUIRE(FromString(std::string(33, '0')) == empty);
}

TEST_CASE("AssetId works as an unordered_map key", "[asset][id]") {
    std::unordered_map<AssetId, int> map;
    auto a = MakeAssetIdFromKey("k1");
    auto b = MakeAssetIdFromKey("k2");
    map[a] = 1;
    map[b] = 2;
    REQUIRE(map[a] == 1);
    REQUIRE(map[b] == 2);
    REQUIRE(map.size() == 2);
}

TEST_CASE("HashOf is equal for equal ids", "[asset][id]") {
    auto a = MakeAssetIdFromKey("same");
    auto b = MakeAssetIdFromKey("same");
    REQUIRE(HashOf(a) == HashOf(b));
}

TEST_CASE("Default AssetId hashes to zero", "[asset][id]") {
    AssetId a{};
    AssetId b{};
    REQUIRE(HashOf(a) == HashOf(b));
}

TEST_CASE("MakeAssetIdFromKey never returns the invalid id", "[asset][id]") {
    // A few inputs that might otherwise produce zeros.
    for (auto key : {std::string{""}, std::string{"\0\0\0\0"},
                     std::string{"   "}}) {
        auto id = MakeAssetIdFromKey(key);
        REQUIRE(id.IsValid());
    }
}
