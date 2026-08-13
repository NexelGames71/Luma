// AssetScanner — recursive walk, folder rows, type inference, ignore rules.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "Luma/Asset/AssetScanner.h"
#include "Luma/Asset/AssetType.h"

using namespace Luma;
namespace fs = std::filesystem;

namespace {
class TempDir {
public:
    TempDir() {
        auto base = fs::temp_directory_path();
        m_path = base / ("luma_asset_test_" + std::to_string(GetTick()));
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

TEST_CASE("AssetScanner::ShouldIgnore flags dotfiles", "[asset][scan]") {
    REQUIRE(AssetScanner::ShouldIgnore("/content/.DS_Store"));
    REQUIRE(AssetScanner::ShouldIgnore("/content/.git/config"));
    REQUIRE_FALSE(AssetScanner::ShouldIgnore("/content/textures/x.png"));
    REQUIRE(AssetScanner::ShouldIgnore("/content/foo.luma_temp"));
    REQUIRE(AssetScanner::ShouldIgnore("/content/foo~"));
}

TEST_CASE("Scan finds files and folders recursively", "[asset][scan]") {
    TempDir dir;
    dir.Touch("textures/hero.png");
    dir.Touch("textures/normal.png");
    dir.Touch("models/tree.obj");
    dir.Touch("README.txt");
    dir.Touch("scratch.tmp~");

    auto entries = AssetScanner::Scan(dir.Path());
    INFO("count = " << entries.size());
    for (const auto& e : entries) {
        INFO("  " << e.packagePath.string() << "  type="
             << static_cast<int>(e.type) << "  stem=" << e.assetName);
    }
    // 2 dirs (textures, models) + 4 files = 6 (root is not emitted as a
    // self-entry by the recursive iterator).
    REQUIRE(entries.size() == 6);

    bool foundHeroPng = false;
    bool foundTreeObj = false;
    for (const auto& e : entries) {
        if (e.assetName == "hero" && e.type == AssetType::Texture)
            foundHeroPng = true;
        if (e.assetName == "tree" && e.type == AssetType::Mesh)
            foundTreeObj = true;
        REQUIRE_FALSE(e.packagePath.empty());
        REQUIRE(e.id.IsValid());
    }
    REQUIRE(foundHeroPng);
    REQUIRE(foundTreeObj);
}

TEST_CASE("Scan infers type from extension", "[asset][scan][type]") {
    TempDir dir;
    dir.Touch("hero.png");
    dir.Touch("wall.jpg");
    dir.Touch("tree.obj");
    dir.Touch("helmet.glb");
    dir.Touch("script.lua");
    dir.Touch("main.luma");
    dir.Touch("wall.lumat");
    dir.Touch("p.hlsl");
    dir.Touch("body.ttf");
    dir.Touch("sfx.wav");
    dir.Touch("notes.unknown");

    auto entries = AssetScanner::Scan(dir.Path());
    auto typeOf = [&](const std::string& stem) {
        for (const auto& e : entries)
            if (e.assetName == stem) return e.type;
        return AssetType::Unknown;
    };
    REQUIRE(typeOf("hero") == AssetType::Texture);
    REQUIRE(typeOf("wall") == AssetType::Texture);   // .jpg is found first
    REQUIRE(typeOf("tree") == AssetType::Mesh);
    REQUIRE(typeOf("helmet") == AssetType::Mesh);
    REQUIRE(typeOf("script") == AssetType::Script);
    REQUIRE(typeOf("main") == AssetType::Scene);
    REQUIRE(typeOf("p") == AssetType::Shader);
    REQUIRE(typeOf("body") == AssetType::Font);
    REQUIRE(typeOf("sfx") == AssetType::Sound);
    REQUIRE(typeOf("notes") == AssetType::Unknown);
}

TEST_CASE("Scan lowercases extensions", "[asset][scan]") {
    TempDir dir;
    dir.Touch("Hero.PNG");
    auto entries = AssetScanner::Scan(dir.Path());
    bool ok = false;
    for (const auto& e : entries) {
        if (e.assetName == "Hero") {
            REQUIRE(e.extension == "png");
            REQUIRE(e.type == AssetType::Texture);
            ok = true;
        }
    }
    REQUIRE(ok);
}

TEST_CASE("Scan omits ignored dotfile dirs", "[asset][scan]") {
    TempDir dir;
    dir.Touch(".git/config");
    dir.Touch("visible.txt");
    auto entries = AssetScanner::Scan(dir.Path());
    bool sawVisible = false, sawHidden = false;
    for (const auto& e : entries) {
        if (e.assetName == "visible") sawVisible = true;
        if (e.assetName == "config") sawHidden = true;
    }
    REQUIRE(sawVisible);
    REQUIRE_FALSE(sawHidden);
}

TEST_CASE("Scan honors maxDepth", "[asset][scan]") {
    TempDir dir;
    dir.Touch("a/b/c/deep.png");
    dir.Touch("shallow.png");
    ScanOptions opts;
    opts.maxDepth = 1;  // depth 0 = root, depth 1 = direct child of root
    auto entries = AssetScanner::Scan(dir.Path(), opts);
    bool sawShallow = false, sawDeep = false;
    for (const auto& e : entries) {
        if (e.assetName == "shallow") sawShallow = true;
        if (e.assetName == "deep") sawDeep = true;
    }
    REQUIRE(sawShallow);
    REQUIRE_FALSE(sawDeep);
}

TEST_CASE("ScanStreaming fires onEntry per row", "[asset][scan]") {
    TempDir dir;
    dir.Touch("a.png");
    dir.Touch("b.png");
    int count = 0;
    AssetScanner::ScanStreaming(dir.Path(), [&](const AssetData& d) {
        ++count;
        REQUIRE(d.id.IsValid());
    });
    REQUIRE(count >= 2);
}

TEST_CASE("Scan on missing root returns empty", "[asset][scan]") {
    auto entries = AssetScanner::Scan("/nonexistent/luma_test_path_xyz");
    REQUIRE(entries.empty());
}
