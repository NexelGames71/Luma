#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"

// Luma native mesh format representation
// This is the engine-internal mesh data structure that gets serialized to .lmesh files
// Designed to be efficient for the Vulkan renderer while allowing future expansion

namespace Luma {

// Vertex data structure for .lmesh format
struct LumaVertex {
    Math::Vec3 position;      // XYZ position
    Math::Vec3 normal;        // Normal vector
    Math::Vec3 tangent;       // Tangent for normal mapping
    Math::Vec3 bitangent;     // Bitangent for normal mapping
    Math::Vec2 texCoord;      // UV coordinates
    Math::Vec4 color;         // Vertex color (RGBA)
    
    // For future expansion: bone indices and weights for skeletal animation
    // u8 boneIndices[4];
    // f32 boneWeights[4];
};

// Bounding volume information
struct LumaBounds {
    Math::Vec3 min;
    Math::Vec3 max;
    Math::Vec3 center;
    f32 radius;
    
    void ComputeFromVertices(const LumaVertex* vertices, usize vertexCount);
};

// Submesh represents a single mesh with a single material
struct LumaSubmesh {
    u32 indexOffset;      // Offset into the global index buffer
    u32 indexCount;       // Number of indices for this submesh
    u32 vertexOffset;     // Offset into the global vertex buffer
    u32 vertexCount;      // Number of vertices for this submesh
    std::string materialName;  // Material reference (for future material system)
    
    // Local bounds for this submesh
    LumaBounds bounds;
};

// Node hierarchy for transform information (for skeletal animation future)
struct LumaNode {
    std::string name;
    i32 parentIndex;      // -1 for root
    Math::Mat4 transform;  // Local transform
    std::vector<i32> childIndices;
};

// Complete Luma mesh data structure
struct LumaMeshData {
    // Header information
    u32 version = 1;      // Format version for compatibility
    std::string name;     // Mesh name
    
    // Vertex and index data
    std::vector<LumaVertex> vertices;
    std::vector<u32> indices;
    
    // Submesh information
    std::vector<LumaSubmesh> submeshes;
    
    // Node hierarchy (for skeletal animation - can be empty for static meshes)
    std::vector<LumaNode> nodes;
    
    // Global bounds
    LumaBounds bounds;
    
    // Import metadata
    std::string sourceFile;     // Original source file path
    std::string importerVersion; // Importer version that created this
    
    // Validation and utility functions
    bool IsValid() const;
    void ComputeBounds();
    void Optimize();  // Vertex cache optimization, etc.
    usize GetTriangleCount() const { return indices.size() / 3; }
    usize GetVertexCount() const { return vertices.size(); }
};

// .lmesh file format specification:
// 
// File Header (16 bytes):
// - Magic: "LMESH" (4 bytes)
// - Version: u32 (4 bytes)
// - Flags: u32 (4 bytes) - bit flags for compression, etc.
// - Reserved: u32 (4 bytes)
//
// Mesh Header (variable):
// - NameLength: u32
// - Name: char[NameLength]
// - VertexCount: u32
// - IndexCount: u32
// - SubmeshCount: u32
// - NodeCount: u32
// - Bounds: 6 * f32 (min, max, center, radius)
//
// Vertex Data:
// - vertices[VertexCount] * sizeof(LumaVertex)
//
// Index Data:
// - indices[IndexCount] * sizeof(u32)
//
// Submesh Data:
// - For each submesh:
//   - indexOffset: u32
//   - indexCount: u32
//   - vertexOffset: u32
//   - vertexCount: u32
//   - materialNameLength: u32
//   - materialName: char[materialNameLength]
//   - bounds: 6 * f32
//
// Node Data (if NodeCount > 0):
// - For each node:
//   - nameLength: u32
//   - name: char[nameLength]
//   - parentIndex: i32
//   - transform: 16 * f32 (Mat4)
//   - childCount: u32
//   - childIndices: childCount * i32

namespace LumaMeshIO {

// Serialize LumaMeshData to .lmesh binary format
bool WriteMesh(const std::filesystem::path& outputPath, const LumaMeshData& mesh);

// Deserialize .lmesh binary format to LumaMeshData
std::optional<LumaMeshData> ReadMesh(const std::filesystem::path& inputPath);

// Validate a .lmesh file (check magic, version, etc.)
bool ValidateMeshFile(const std::filesystem::path& path);

// Get mesh metadata without loading full data
struct MeshMetadata {
    u32 version;
    std::string name;
    u32 vertexCount;
    u32 indexCount;
    u32 submeshCount;
    LumaBounds bounds;
};
std::optional<MeshMetadata> ReadMeshMetadata(const std::filesystem::path& path);

}  // namespace LumaMeshIO

}  // namespace Luma
