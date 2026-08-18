// MeshImporter — end-to-end OBJ support: a real .obj file (vertices + UVs +
// normals) imports to a .lmesh companion that round-trips through
// LumaMeshIO::ReadMesh with the expected geometry.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "Luma/Asset/AssetImportManager.h"
#include "Luma/Asset/LumaMesh.h"
#include "Luma/Asset/LumaMeshFormat.h"
#include "Luma/Asset/MeshImporter.h"
#include "Luma/Asset/MtlParser.h"

using namespace Luma;
namespace fs = std::filesystem;

namespace {
class TempDir {
public:
    TempDir() {
        auto base = fs::temp_directory_path();
        m_path = base / ("luma_mesh_test_" + std::to_string(GetTick()));
        fs::create_directories(m_path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(m_path, ec);
    }
    const fs::path& Path() const { return m_path; }

    // Writes a small quad OBJ (2 triangles, 4 vertices) with UVs + normals.
    void WriteQuadObj(const fs::path& rel) {
        auto full = m_path / rel;
        fs::create_directories(full.parent_path());
        std::ofstream out(full);
        out << "# luma test quad\n"
            << "v -0.5 0.0 -0.5\n"
            << "v  0.5 0.0 -0.5\n"
            << "v  0.5 0.0  0.5\n"
            << "v -0.5 0.0  0.5\n"
            << "vt 0 0\n"
            << "vt 1 0\n"
            << "vt 1 1\n"
            << "vt 0 1\n"
            << "vn 0 1 0\n"
            << "f 1/1/1 2/2/1 3/3/1\n"
            << "f 1/1/1 3/3/1 4/4/1\n";
    }

private:
    static int GetTick() {
        static int n = 0;
        return ++n;
    }
    fs::path m_path;
};
}  // namespace

TEST_CASE("MeshImporter: .obj imports to a valid .lmesh companion",
          "[asset][mesh][obj]") {
    TempDir dir;
    dir.WriteQuadObj("models/quad.obj");

    MeshImporter importer;
    REQUIRE(importer.CanImport(dir.Path() / "models/quad.obj"));
    REQUIRE(importer.GetSupportedExtensions().size() > 0);

    auto result = importer.Import(dir.Path() / "models/quad.obj", "",
                                  dir.Path() / "models");
    REQUIRE(result.status == ImportStatus::Completed);
    REQUIRE(result.nativePath == dir.Path() / "models/quad.lmesh");
    REQUIRE(fs::exists(result.nativePath));

    auto data = LumaMeshIO::ReadMesh(result.nativePath);
    REQUIRE(data.has_value());
    REQUIRE(data->IsValid());

    // Assimp keeps per-face vertex combos (v/vt/vn index tuples) unless
    // JoinIdenticalVertices runs, so the 4-corner quad imports as 6 verts
    // (2 triangles x 3) with 6 indices, one submesh. Geometry is intact.
    CHECK(data->vertices.size() == 6);
    CHECK(data->indices.size() == 6);
    CHECK(data->GetTriangleCount() == 2);
    CHECK(data->submeshes.size() == 1);

    // Positions survive: corner at (-0.5, 0, -0.5).
    bool hasCorner = false;
    for (const auto& v : data->vertices) {
        if (std::fabs(v.position.x + 0.5f) < 1e-5f &&
            std::fabs(v.position.y) < 1e-5f &&
            std::fabs(v.position.z + 0.5f) < 1e-5f) {
            hasCorner = true;
        }
        // The OBJ declares +Y normals; importer keeps them (no GenNormals
        // needed) and UVs come through as the source vt values.
        CHECK(std::fabs(v.normal.y - 1.0f) < 1e-4f);
    }
    CHECK(hasCorner);

    // UVs: the quad maps the full [0,1] range.
    bool uvExtent = false;
    for (const auto& v : data->vertices) {
        if (v.texCoord.x >= 0.999f && v.texCoord.y >= 0.999f) uvExtent = true;
        CHECK(v.texCoord.x >= 0.0f);
        CHECK(v.texCoord.x <= 1.0f);
        CHECK(v.texCoord.y >= 0.0f);
        CHECK(v.texCoord.y <= 1.0f);
    }
    CHECK(uvExtent);

    // Bounds are computed from the imported positions.
    CHECK(data->bounds.radius > 0.0f);
}

TEST_CASE("MeshImporter: .obj without normals gets generated ones",
          "[asset][mesh][obj]") {
    TempDir dir;
    // Same quad but no `vn` lines — generateNormals (default on) must fill
    // normals so the mesh renders lit instead of flat black.
    {
        auto full = dir.Path() / "models/plain.obj";
        fs::create_directories(full.parent_path());
        std::ofstream out(full);
        out << "v -0.5 0.0 -0.5\n"
            << "v  0.5 0.0 -0.5\n"
            << "v  0.5 0.0  0.5\n"
            << "v -0.5 0.0  0.5\n"
            << "vt 0 0\n"
            << "vt 1 0\n"
            << "vt 1 1\n"
            << "vt 0 1\n"
            // Counter-clockwise winding (viewed from +Y) so the generated
            // normals point up.
            << "f 1/1 3/3 2/2\n"
            << "f 1/1 4/4 3/3\n";
    }

    MeshImporter importer;
    auto result = importer.Import(dir.Path() / "models/plain.obj", "",
                                  dir.Path() / "models");
    REQUIRE(result.status == ImportStatus::Completed);

    auto data = LumaMeshIO::ReadMesh(result.nativePath);
    REQUIRE(data.has_value());
    REQUIRE(data->vertices.size() == 6);
    for (const auto& v : data->vertices) {
        // Up-facing quad: generated normals point up (within a small cone).
        CHECK(v.normal.y > 0.5f);
    }
}

TEST_CASE("MtlParser: parses classic + PBR material properties", "[asset][mesh][obj][mtl]") {
    TempDir dir;
    auto mtlPath = dir.Path() / "models/wood.mtl";
    fs::create_directories(mtlPath.parent_path());
    {
        std::ofstream out(mtlPath);
        out << "newmtl Wood\n"
            << "Ka 0.1 0.1 0.1\n"
            << "Kd 0.6 0.4 0.2\n"
            << "Ks 0.05 0.05 0.05\n"
            << "Ke 0 0 0\n"
            << "Ns 64\n"
            << "d 1.0\n"
            << "illum 2\n"
            << "map_Kd textures/wood_diff.png\n"
            << "map_Bump -bm 0.5 textures/wood_nor.png\n"
            << "map_Pr textures/wood_rough.png\n"
            << "map_Pm textures/wood_metal.png\n"
            << "\n"
            << "newmtl Metal\n"
            << "Kd 0.8 0.8 0.8\n"
            << "map_Kd metal_diff.png\n";
    }

    auto mats = ParseMtlFile(mtlPath);
    REQUIRE(mats.size() == 2);

    const auto& wood = mats[0];
    CHECK(wood.name == "Wood");
    CHECK(wood.diffuse.x == 0.6f);
    CHECK(wood.diffuse.y == 0.4f);
    CHECK(wood.diffuse.z == 0.2f);
    CHECK(wood.specular.x == 0.05f);
    CHECK(wood.shininess == 64.0f);
    CHECK(wood.opacity == 1.0f);
    CHECK(wood.mapDiffuse == "textures/wood_diff.png");
    CHECK(wood.mapNormal == "textures/wood_nor.png");   // options (-bm) skipped
    CHECK(wood.mapRoughness == "textures/wood_rough.png");
    CHECK(wood.mapMetallic == "textures/wood_metal.png");

    const auto& metal = mats[1];
    CHECK(metal.name == "Metal");
    CHECK(metal.mapDiffuse == "metal_diff.png");
}

TEST_CASE("MtlParser: d/Tr transparency and default material", "[asset][mesh][obj][mtl]") {
    TempDir dir;
    auto mtlPath = dir.Path() / "glass.mtl";
    {
        std::ofstream out(mtlPath);
        out << "newmtl Glass\n"
            << "Kd 0.9 0.9 0.9\n"
            << "d 0.25\n";
    }
    {
        std::ofstream out(dir.Path() / "tr.mtl");
        out << "newmtl TrGlass\n"
            << "Tr 0.3\n";  // 1 - 0.3 = 0.7 opacity
    }
    auto mats = ParseMtlFile(mtlPath);
    REQUIRE(mats.size() == 1);
    CHECK(mats[0].opacity == 0.25f);

    auto tr = ParseMtlFile(dir.Path() / "tr.mtl");
    REQUIRE(tr.size() == 1);
    CHECK(tr[0].opacity == 0.7f);

    // Unreadable / empty file → empty result.
    CHECK(ParseMtlFile(dir.Path() / "missing.mtl").empty());
}

TEST_CASE("MtlParser: FindMtlForObj resolves mtllib + sibling fallback",
          "[asset][mesh][obj][mtl]") {
    TempDir dir;
    // Explicit mtllib path.
    {
        auto obj = dir.Path() / "models/explicit.obj";
        fs::create_directories(obj.parent_path());
        {
            std::ofstream out(obj);
            out << "mtllib materials/wood.mtl\n";
        }  // close (flush) before reading back
        fs::create_directories(dir.Path() / "models/materials");
        {
            std::ofstream out(dir.Path() / "models/materials/wood.mtl");
            out << "newmtl Wood\n";
        }
        CHECK(FindMtlForObj(obj) == dir.Path() / "models/materials/wood.mtl");
    }
    // No mtllib → sibling <stem>.mtl.
    {
        auto obj = dir.Path() / "models/plain.obj";
        fs::create_directories(obj.parent_path());
        {
            std::ofstream out(obj);
            out << "v 0 0 0\n";
        }
        {
            std::ofstream out(dir.Path() / "models/plain.mtl");
            out << "newmtl M\n";
        }
        CHECK(FindMtlForObj(obj) == dir.Path() / "models/plain.mtl");
    }
    // No MTL at all → empty.
    {
        auto obj = dir.Path() / "models/bare.obj";
        fs::create_directories(obj.parent_path());
        std::ofstream(obj) << "v 0 0 0\n";
        CHECK(FindMtlForObj(obj).empty());
    }
}

TEST_CASE("MeshImporter: OBJ submesh carries the .mtl material name",
          "[asset][mesh][obj][mtl]") {
    TempDir dir;
    dir.WriteQuadObj("models/quad.obj");
    // Give the quad a material reference via mtllib + usemtl.
    {
        auto obj = dir.Path() / "models/quad.obj";
        std::ofstream mtl(dir.Path() / "models/quad.mtl");
        mtl << "newmtl WoodMat\n" << "Kd 0.6 0.4 0.2\n";
        std::ofstream out(obj);
        out << "mtllib quad.mtl\n"
            << "v -0.5 0.0 -0.5\n"
            << "v  0.5 0.0 -0.5\n"
            << "v  0.5 0.0  0.5\n"
            << "v -0.5 0.0  0.5\n"
            << "vt 0 0\n"
            << "vt 1 0\n"
            << "vt 1 1\n"
            << "vt 0 1\n"
            << "usemtl WoodMat\n"
            << "f 1/1 2/2 3/3\n"
            << "f 1/1 3/3 4/4\n";
    }

    MeshImporter importer;
    auto result = importer.Import(dir.Path() / "models/quad.obj", "",
                                  dir.Path() / "models");
    REQUIRE(result.status == ImportStatus::Completed);

    auto data = LumaMeshIO::ReadMesh(result.nativePath);
    REQUIRE(data.has_value());
    REQUIRE(data->submeshes.size() >= 1);
    CHECK(data->submeshes[0].materialName == "WoodMat");
}

TEST_CASE("MeshImporter: rejects non-mesh files", "[asset][mesh][obj]") {
    TempDir dir;
    auto txt = dir.Path() / "note.txt";
    std::ofstream(txt) << "hello";

    MeshImporter importer;
    CHECK_FALSE(importer.CanImport(txt));
    CHECK_FALSE(importer.ValidateSource(txt));
}
