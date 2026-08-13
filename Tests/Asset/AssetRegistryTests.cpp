// AssetRegistry — roots, scan, lookup by id/path, filters, incremental
// refresh after file watcher events.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <set>

#include "Luma/Asset/AssetRegistry.h"

using namespace Luma;
namespace fs = std::filesystem;

namespace {
class TempDir {
public:
    TempDir() {
        auto base = fs::temp_directory_path();
        m_path = base / ("luma_registry_test_" + std::to_string(GetTick()));
        fs::create_directories(m_path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(m_path, ec);
    }
    const fs::path& Path() const { return m_path; }
    void Touch(const fs::path& rel, const std::string& body = "x") {
        auto full = m_path / rel;
        fs::create_directories(full.parent_path());
        std::ofstream(full) << body;
    }

private:
    static int GetTick() {
        static int n = 0;
        return ++n;
    }
    fs::path m_path;
};
}  // namespace

TEST_CASE("Empty registry has zero entries", "[asset][registry]") {
    AssetRegistry r;
    REQUIRE(r.Size() == 0);
    REQUIRE(r.All().empty());
    REQUIRE(r.Roots().empty());
}

TEST_CASE("AddRoot + Scan indexes files and folders", "[asset][registry]") {
    TempDir dir;
    dir.Touch("textures/hero.png");
    dir.Touch("models/tree.obj");
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    REQUIRE(r.Size() >= 4);  // root, textures, models, 2 files
    REQUIRE(r.Roots().size() == 1);
    auto all = r.All();
    bool sawHero = false, sawTree = false;
    for (const auto* a : all) {
        if (a->assetName == "hero" && a->type == AssetType::Texture)
            sawHero = true;
        if (a->assetName == "tree" && a->type == AssetType::Mesh)
            sawTree = true;
    }
    REQUIRE(sawHero);
    REQUIRE(sawTree);
}

TEST_CASE("AddRoot is idempotent", "[asset][registry]") {
    TempDir dir;
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.AddRoot(dir.Path());
    REQUIRE(r.Roots().size() == 1);
}

TEST_CASE("LookupByPath returns the same id as MakeAssetIdFromKey",
          "[asset][registry]") {
    TempDir dir;
    dir.Touch("a/b/hero.png");
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    auto abs = (dir.Path() / "a" / "b" / "hero.png");
    auto key = r.Salt() + "|" + abs.lexically_normal().string();
    AssetId expected = MakeAssetIdFromKey(key);
    const AssetData* a = r.LookupByPath(abs);
    REQUIRE(a != nullptr);
    REQUIRE(a->id == expected);
}

TEST_CASE("FilterByType returns only matching types", "[asset][registry]") {
    TempDir dir;
    dir.Touch("a/hero.png");
    dir.Touch("a/wall.png");
    dir.Touch("a/tree.obj");
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    auto textures = r.FilterByType(AssetType::Texture);
    REQUIRE(textures.size() == 2);
    for (const auto* a : textures) REQUIRE(a->type == AssetType::Texture);
}

TEST_CASE("FilterByName is case-insensitive substring", "[asset][registry]") {
    TempDir dir;
    dir.Touch("Hero.png");
    dir.Touch("hero_alt.png");
    dir.Touch("Wall.png");
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    auto hits = r.FilterByName("HERO");
    REQUIRE(hits.size() == 2);
}

TEST_CASE("FilterByDirectory returns assets under the directory",
          "[asset][registry]") {
    TempDir dir;
    dir.Touch("a/b/c/deep.png");
    dir.Touch("a/b/shallow.png");
    dir.Touch("a/outside.png");
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    auto inside = r.FilterByDirectory(dir.Path() / "a" / "b");
    REQUIRE(inside.size() == 2);
}

TEST_CASE("Composite Filter combines type + dir + name",
          "[asset][registry]") {
    TempDir dir;
    dir.Touch("x/hero.png");
    dir.Touch("x/hero.obj");
    dir.Touch("y/wall.png");
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    auto hits = r.Filter(AssetType::Texture, dir.Path() / "x", "hero");
    REQUIRE(hits.size() == 1);
    REQUIRE(hits[0]->type == AssetType::Texture);
    REQUIRE(hits[0]->assetName == "hero");
}

TEST_CASE("Clear empties the index but keeps roots", "[asset][registry]") {
    TempDir dir;
    dir.Touch("a.png");
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    REQUIRE(r.Size() > 0);
    r.Clear();
    REQUIRE(r.Size() == 0);
    REQUIRE(r.Roots().size() == 1);
}

TEST_CASE("RefreshPath adds a newly-created file", "[asset][registry]") {
    TempDir dir;
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    auto before = r.Size();
    dir.Touch("fresh.png");
    r.RefreshPath(dir.Path() / "fresh.png");
    REQUIRE(r.Size() == before + 1);
    const AssetData* a = r.LookupByPath(dir.Path() / "fresh.png");
    REQUIRE(a != nullptr);
    REQUIRE(a->type == AssetType::Texture);
}

TEST_CASE("RefreshPath drops a deleted file", "[asset][registry]") {
    TempDir dir;
    dir.Touch("gonow.png");
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    REQUIRE(r.LookupByPath(dir.Path() / "gonow.png") != nullptr);
    std::error_code ec;
    fs::remove(dir.Path() / "gonow.png", ec);
    r.RefreshPath(dir.Path() / "gonow.png");
    REQUIRE(r.LookupByPath(dir.Path() / "gonow.png") == nullptr);
}

TEST_CASE("RemoveUnder clears every entry under a directory",
          "[asset][registry]") {
    TempDir dir;
    dir.Touch("drop/a.png");
    dir.Touch("drop/b.png");
    dir.Touch("keep/c.png");
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    // root + drop + keep + a + b + c = 5 minimum; registry may also include
    // the drop/ parent dir itself depending on the order.
    REQUIRE(r.Size() >= 5);
    r.RemoveUnder(dir.Path() / "drop");
    REQUIRE(r.LookupByPath(dir.Path() / "drop" / "a.png") == nullptr);
    REQUIRE(r.LookupByPath(dir.Path() / "drop" / "b.png") == nullptr);
    REQUIRE(r.LookupByPath(dir.Path() / "keep" / "c.png") != nullptr);
}

TEST_CASE("SetSalt changes all ids after re-scan", "[asset][registry]") {
    TempDir dir;
    dir.Touch("a.png");
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    auto oldId = r.LookupByPath(dir.Path() / "a.png")->id;
    r.SetSalt("different-salt");
    r.Scan();
    auto newId = r.LookupByPath(dir.Path() / "a.png")->id;
    REQUIRE(oldId != newId);
}

TEST_CASE("Multiple roots are all indexed", "[asset][registry]") {
    TempDir a, b;
    a.Touch("a.png");
    b.Touch("b.png");
    AssetRegistry r;
    r.AddRoot(a.Path());
    r.AddRoot(b.Path());
    r.Scan();
    REQUIRE(r.LookupByPath(a.Path() / "a.png") != nullptr);
    REQUIRE(r.LookupByPath(b.Path() / "b.png") != nullptr);
}

TEST_CASE("DisplayPathFor strips the root and prefixes 'Content/'",
          "[asset][registry][display]") {
    TempDir dir;
    dir.Touch("Textures/hero.png");
    AssetRegistry r;
    r.AddRoot(dir.Path());
    r.Scan();
    auto abs = dir.Path() / "Textures" / "hero.png";
    auto display = r.DisplayPathFor(abs);
    REQUIRE(display == "Content/Textures/hero.png");
}

TEST_CASE("DisplayPathFor falls back to the absolute path when no root matches",
          "[asset][registry][display]") {
    AssetRegistry r;
    auto display = r.DisplayPathFor(std::filesystem::path("C:/elsewhere/x.png"));
    // Display paths always use forward slashes (mirrors Unreal/Unity
    // conventions) even on Windows, where the OS path uses '\\'.
    REQUIRE(display.find("C:/elsewhere/x.png") != std::string::npos);
}

TEST_CASE("DisplayPathFor handles the root itself", "[asset][registry][display]") {
    TempDir dir;
    AssetRegistry r;
    r.AddRoot(dir.Path());
    auto display = r.DisplayPathFor(dir.Path());
    REQUIRE(display == "Content/");
}
