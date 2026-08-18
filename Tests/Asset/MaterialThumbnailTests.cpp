// MaterialThumbnailRenderer tests: a real .lmat file (MaterialSerializer
// layout) must render to a valid PNG, and the material's base color must
// actually drive the preview sphere (two differently-colored materials
// produce different thumbnails).

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include "Luma/Asset/MaterialThumbnailRenderer.h"

namespace fs = std::filesystem;
using Luma::AssetId;
using Luma::MaterialThumbnailRenderer;
using Luma::u32;
using Luma::u8;

namespace {

fs::path TempPath(const char* name) {
    static int counter = 0;
    return fs::temp_directory_path() /
           ("luma_mat_thumb_" + std::string(name) + "_" +
            std::to_string(counter++) + ".lmat");
}

fs::path TempOut(const char* name) {
    static int counter = 0;
    return fs::temp_directory_path() /
           ("luma_mat_thumb_" + std::string(name) + "_out_" +
            std::to_string(counter++) + ".png");
}

void WriteLmat(const fs::path& path, const char* name, float r, float g,
               float b, float metallic, float roughness) {
    std::string json =
        "{\n"
        "  \"version\": 1,\n"
        "  \"name\": \"" + std::string(name) + "\",\n"
        "  \"blendMode\": 0,\n"
        "  \"opacityThreshold\": 0.5,\n"
        "  \"constants\": {\n"
        "    \"baseColor\": [" + std::to_string(r) + ", " +
        std::to_string(g) + ", " + std::to_string(b) + "],\n"
        "    \"metallic\": " + std::to_string(metallic) + ",\n"
        "    \"roughness\": " + std::to_string(roughness) + ",\n"
        "    \"normal\": [0.5, 0.5, 1.0],\n"
        "    \"emissive\": [0.0, 0.0, 0.0],\n"
        "    \"opacity\": 1.0\n"
        "  },\n"
        "  \"properties\": {},\n"
        "  \"graph\": { \"nextId\": 1, \"nodes\": [] }\n"
        "}\n";
    std::ofstream f(path, std::ios::binary);
    f.write(json.data(), static_cast<std::streamsize>(json.size()));
}

bool ReadFile(const fs::path& path, std::vector<char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f),
               std::istreambuf_iterator<char>());
    return !out.empty();
}

// PNG IHDR width/height live at byte offset 16..23 (big-endian).
u32 BeU32(const char* p) {
    return (static_cast<u32>(static_cast<u8>(p[0])) << 24) |
           (static_cast<u32>(static_cast<u8>(p[1])) << 16) |
           (static_cast<u32>(static_cast<u8>(p[2])) << 8) |
           static_cast<u32>(static_cast<u8>(p[3]));
}

}  // namespace

TEST_CASE("MaterialThumbnailRenderer renders a real .lmat to a PNG",
          "[asset][thumbnail]") {
    fs::path mat = TempPath("red");
    fs::path out = TempOut("red");
    WriteLmat(mat, "RedMat", 0.85f, 0.1f, 0.1f, 0.0f, 0.35f);

    MaterialThumbnailRenderer renderer;
    REQUIRE(renderer.CanRender(AssetId{}, mat));
    REQUIRE(renderer.RenderThumbnail(AssetId{}, mat, out, 128, 128));

    REQUIRE(fs::exists(out));
    std::vector<char> bytes;
    REQUIRE(ReadFile(out, bytes));
    // PNG signature + IHDR: dimensions must be 128x128.
    REQUIRE(bytes.size() > 32);
    REQUIRE(static_cast<u8>(bytes[0]) == 0x89);
    REQUIRE(BeU32(bytes.data() + 16) == 128);
    REQUIRE(BeU32(bytes.data() + 20) == 128);
    // A 128x128 shaded sphere is far larger than an empty/minimal file.
    REQUIRE(bytes.size() > 512);

    fs::remove(mat);
    fs::remove(out);
}

TEST_CASE("MaterialThumbnailRenderer base color drives the sphere",
          "[asset][thumbnail]") {
    fs::path red = TempPath("red");
    fs::path blue = TempPath("blue");
    fs::path outRed = TempOut("red");
    fs::path outBlue = TempOut("blue");
    WriteLmat(red, "RedMat", 0.85f, 0.1f, 0.1f, 0.0f, 0.35f);
    WriteLmat(blue, "BlueMat", 0.1f, 0.1f, 0.85f, 0.0f, 0.35f);

    MaterialThumbnailRenderer renderer;
    REQUIRE(renderer.RenderThumbnail(AssetId{}, red, outRed, 128, 128));
    REQUIRE(renderer.RenderThumbnail(AssetId{}, blue, outBlue, 128, 128));

    std::vector<char> redBytes, blueBytes;
    REQUIRE(ReadFile(outRed, redBytes));
    REQUIRE(ReadFile(outBlue, blueBytes));
    // Same lighting/model, different material color -> different pixels.
    REQUIRE(redBytes != blueBytes);

    fs::remove(red);
    fs::remove(blue);
    fs::remove(outRed);
    fs::remove(outBlue);
}

TEST_CASE("MaterialThumbnailRenderer CanRender rejects non-material files",
          "[asset][thumbnail]") {
    MaterialThumbnailRenderer renderer;
    REQUIRE_FALSE(renderer.CanRender(AssetId{}, "D:/nonexistent.png"));
    REQUIRE_FALSE(renderer.CanRender(AssetId{}, "D:/nonexistent.lmesh"));

    // A missing .lmat cannot be rendered.
    REQUIRE_FALSE(renderer.RenderThumbnail(
        AssetId{}, "D:/luma_missing_material.lmat", "D:/out.png", 128, 128));
}
