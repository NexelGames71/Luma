#include "Luma/Asset/LumaMesh.h"

#include <algorithm>
#include <fstream>
#include <cstring>
#include <filesystem>

#include "Luma/Core/Log.h"
#include "Luma/Math/Math.h"
#include "Luma/Math/MathUtils.h"

namespace Luma {

void LumaBounds::ComputeFromVertices(const LumaVertex* vertices, usize vertexCount) {
    if (vertexCount == 0) {
        min = Math::Vec3{0.0f, 0.0f, 0.0f};
        max = Math::Vec3{0.0f, 0.0f, 0.0f};
        center = Math::Vec3{0.0f, 0.0f, 0.0f};
        radius = 0.0f;
        return;
    }
    
    // Initialize min/max to first vertex
    min = vertices[0].position;
    max = vertices[0].position;
    
    // Find min/max
    for (usize i = 1; i < vertexCount; ++i) {
        const auto& pos = vertices[i].position;
        min.x = std::min(min.x, pos.x);
        min.y = std::min(min.y, pos.y);
        min.z = std::min(min.z, pos.z);
        max.x = std::max(max.x, pos.x);
        max.y = std::max(max.y, pos.y);
        max.z = std::max(max.z, pos.z);
    }
    
    // Compute center and radius
    center = (min + max) * 0.5f;
    radius = 0.0f;
    
    for (usize i = 0; i < vertexCount; ++i) {
        Math::Vec3 diff = vertices[i].position - center;
        f32 dist = Math::Length(diff);
        radius = std::max(radius, dist);
    }
}

bool LumaMeshData::IsValid() const {
    if (vertices.empty()) return false;
    if (indices.empty()) return false;
    if (indices.size() % 3 != 0) return false;  // Must be triangles
    if (submeshes.empty()) return false;
    
    // Validate submesh ranges
    for (const auto& submesh : submeshes) {
        if (submesh.indexOffset + submesh.indexCount > indices.size()) return false;
        if (submesh.vertexOffset + submesh.vertexCount > vertices.size()) return false;
    }
    
    return true;
}

void LumaMeshData::ComputeBounds() {
    bounds.ComputeFromVertices(vertices.data(), vertices.size());
    
    // Compute submesh bounds
    for (auto& submesh : submeshes) {
        if (submesh.vertexCount > 0) {
            submesh.bounds.ComputeFromVertices(
                vertices.data() + submesh.vertexOffset,
                submesh.vertexCount);
        }
    }
}

void LumaMeshData::Optimize() {
    // TODO: Implement vertex cache optimization, vertex fetch optimization, etc.
    // For now, this is a placeholder for future optimization passes
}

namespace LumaMeshIO {

namespace {
    constexpr u32 kMagic = 0x4853454D;  // "MESH" in little-endian
    constexpr u32 kCurrentVersion = 1;
    
    struct FileHeader {
        u32 magic;
        u32 version;
        u32 flags;
        u32 reserved;
    };
    
    bool WriteString(std::ofstream& file, const std::string& str) {
        u32 length = static_cast<u32>(str.size());
        file.write(reinterpret_cast<const char*>(&length), sizeof(length));
        if (!str.empty()) {
            file.write(str.data(), length);
        }
        return file.good();
    }
    
    bool ReadString(std::ifstream& file, std::string& str) {
        u32 length;
        file.read(reinterpret_cast<char*>(&length), sizeof(length));
        if (!file.good()) return false;
        
        str.resize(length);
        if (length > 0) {
            file.read(&str[0], length);
        }
        return file.good();
    }
    
    bool WriteVec3(std::ofstream& file, const Math::Vec3& v) {
        file.write(reinterpret_cast<const char*>(&v.x), sizeof(f32) * 3);
        return file.good();
    }
    
    bool ReadVec3(std::ifstream& file, Math::Vec3& v) {
        file.read(reinterpret_cast<char*>(&v.x), sizeof(f32) * 3);
        return file.good();
    }
    
    bool WriteMat4(std::ofstream& file, const Math::Mat4& m) {
        file.write(reinterpret_cast<const char*>(&m.m[0]), sizeof(f32) * 16);
        return file.good();
    }
    
    bool ReadMat4(std::ifstream& file, Math::Mat4& m) {
        file.read(reinterpret_cast<char*>(&m.m[0]), sizeof(f32) * 16);
        return file.good();
    }
}

bool WriteMesh(const std::filesystem::path& outputPath, const LumaMeshData& mesh) {
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        LUMA_LOG_ERROR("LumaMeshIO", "Failed to open file for writing: {}", outputPath.string());
        return false;
    }
    
    // Write file header
    FileHeader header;
    header.magic = kMagic;
    header.version = kCurrentVersion;
    header.flags = 0;  // No compression for now
    header.reserved = 0;
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!file.good()) return false;
    
    // Write mesh header
    WriteString(file, mesh.name);
    
    u32 vertexCount = static_cast<u32>(mesh.vertices.size());
    u32 indexCount = static_cast<u32>(mesh.indices.size());
    u32 submeshCount = static_cast<u32>(mesh.submeshes.size());
    u32 nodeCount = static_cast<u32>(mesh.nodes.size());
    
    file.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
    file.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
    file.write(reinterpret_cast<const char*>(&submeshCount), sizeof(submeshCount));
    file.write(reinterpret_cast<const char*>(&nodeCount), sizeof(nodeCount));
    
    // Write bounds
    WriteVec3(file, mesh.bounds.min);
    WriteVec3(file, mesh.bounds.max);
    WriteVec3(file, mesh.bounds.center);
    file.write(reinterpret_cast<const char*>(&mesh.bounds.radius), sizeof(f32));
    
    if (!file.good()) return false;
    
    // Write vertex data
    file.write(reinterpret_cast<const char*>(mesh.vertices.data()), 
              mesh.vertices.size() * sizeof(LumaVertex));
    
    // Write index data
    file.write(reinterpret_cast<const char*>(mesh.indices.data()), 
              mesh.indices.size() * sizeof(u32));
    
    // Write submesh data
    for (const auto& submesh : mesh.submeshes) {
        file.write(reinterpret_cast<const char*>(&submesh.indexOffset), sizeof(u32));
        file.write(reinterpret_cast<const char*>(&submesh.indexCount), sizeof(u32));
        file.write(reinterpret_cast<const char*>(&submesh.vertexOffset), sizeof(u32));
        file.write(reinterpret_cast<const char*>(&submesh.vertexCount), sizeof(u32));
        WriteString(file, submesh.materialName);
        
        // Write submesh bounds
        WriteVec3(file, submesh.bounds.min);
        WriteVec3(file, submesh.bounds.max);
        WriteVec3(file, submesh.bounds.center);
        file.write(reinterpret_cast<const char*>(&submesh.bounds.radius), sizeof(f32));
    }
    
    if (!file.good()) return false;
    
    // Write node data (if any)
    for (const auto& node : mesh.nodes) {
        WriteString(file, node.name);
        file.write(reinterpret_cast<const char*>(&node.parentIndex), sizeof(i32));
        WriteMat4(file, node.transform);
        
        u32 childCount = static_cast<u32>(node.childIndices.size());
        file.write(reinterpret_cast<const char*>(&childCount), sizeof(u32));
        
        for (i32 childIndex : node.childIndices) {
            file.write(reinterpret_cast<const char*>(&childIndex), sizeof(i32));
        }
    }
    
    if (!file.good()) return false;
    
    file.close();
    LUMA_LOG_INFO("LumaMeshIO", "Wrote mesh to: {} ({} vertices, {} indices)", 
                  outputPath.string(), vertexCount, indexCount);
    return true;
}

std::optional<LumaMeshData> ReadMesh(const std::filesystem::path& inputPath) {
    std::ifstream file(inputPath, std::ios::binary);
    if (!file.is_open()) {
        LUMA_LOG_ERROR("LumaMeshIO", "Failed to open file for reading: {}", inputPath.string());
        return std::nullopt;
    }
    
    // Read and validate file header
    FileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file.good() || header.magic != kMagic) {
        LUMA_LOG_ERROR("LumaMeshIO", "Invalid file magic in: {}", inputPath.string());
        return std::nullopt;
    }
    
    if (header.version > kCurrentVersion) {
        LUMA_LOG_WARN("LumaMeshIO", "File version {} newer than current version {}", 
                      header.version, kCurrentVersion);
    }
    
    LumaMeshData mesh;
    mesh.version = header.version;
    
    // Read mesh header
    ReadString(file, mesh.name);
    
    u32 vertexCount, indexCount, submeshCount, nodeCount;
    file.read(reinterpret_cast<char*>(&vertexCount), sizeof(u32));
    file.read(reinterpret_cast<char*>(&indexCount), sizeof(u32));
    file.read(reinterpret_cast<char*>(&submeshCount), sizeof(u32));
    file.read(reinterpret_cast<char*>(&nodeCount), sizeof(u32));
    
    // Read bounds
    ReadVec3(file, mesh.bounds.min);
    ReadVec3(file, mesh.bounds.max);
    ReadVec3(file, mesh.bounds.center);
    file.read(reinterpret_cast<char*>(&mesh.bounds.radius), sizeof(f32));
    
    if (!file.good()) return std::nullopt;
    
    // Read vertex data
    mesh.vertices.resize(vertexCount);
    file.read(reinterpret_cast<char*>(mesh.vertices.data()), 
              vertexCount * sizeof(LumaVertex));
    
    // Read index data
    mesh.indices.resize(indexCount);
    file.read(reinterpret_cast<char*>(mesh.indices.data()), 
              indexCount * sizeof(u32));
    
    // Read submesh data
    mesh.submeshes.resize(submeshCount);
    for (auto& submesh : mesh.submeshes) {
        file.read(reinterpret_cast<char*>(&submesh.indexOffset), sizeof(u32));
        file.read(reinterpret_cast<char*>(&submesh.indexCount), sizeof(u32));
        file.read(reinterpret_cast<char*>(&submesh.vertexOffset), sizeof(u32));
        file.read(reinterpret_cast<char*>(&submesh.vertexCount), sizeof(u32));
        ReadString(file, submesh.materialName);
        
        // Read submesh bounds
        ReadVec3(file, submesh.bounds.min);
        ReadVec3(file, submesh.bounds.max);
        ReadVec3(file, submesh.bounds.center);
        file.read(reinterpret_cast<char*>(&submesh.bounds.radius), sizeof(f32));
    }
    
    if (!file.good()) return std::nullopt;
    
    // Read node data (if any)
    mesh.nodes.resize(nodeCount);
    for (auto& node : mesh.nodes) {
        ReadString(file, node.name);
        file.read(reinterpret_cast<char*>(&node.parentIndex), sizeof(i32));
        ReadMat4(file, node.transform);
        
        u32 childCount;
        file.read(reinterpret_cast<char*>(&childCount), sizeof(u32));
        
        node.childIndices.resize(childCount);
        for (u32 i = 0; i < childCount; ++i) {
            file.read(reinterpret_cast<char*>(&node.childIndices[i]), sizeof(i32));
        }
    }
    
    if (!file.good()) return std::nullopt;
    
    file.close();
    
    if (!mesh.IsValid()) {
        LUMA_LOG_ERROR("LumaMeshIO", "Invalid mesh data in: {}", inputPath.string());
        return std::nullopt;
    }
    
    LUMA_LOG_INFO("LumaMeshIO", "Read mesh from: {} ({} vertices, {} indices)", 
                  inputPath.string(), vertexCount, indexCount);
    return mesh;
}

bool ValidateMeshFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    FileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    return (file.good() && header.magic == kMagic && 
            header.version <= kCurrentVersion);
}

std::optional<MeshMetadata> ReadMeshMetadata(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    // Read file header
    FileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file.good() || header.magic != kMagic) {
        return std::nullopt;
    }
    
    MeshMetadata metadata;
    metadata.version = header.version;
    
    // Read mesh name
    ReadString(file, metadata.name);
    
    // Read counts
    u32 vertexCount, indexCount, submeshCount, nodeCount;
    file.read(reinterpret_cast<char*>(&vertexCount), sizeof(u32));
    file.read(reinterpret_cast<char*>(&indexCount), sizeof(u32));
    file.read(reinterpret_cast<char*>(&submeshCount), sizeof(u32));
    file.read(reinterpret_cast<char*>(&nodeCount), sizeof(u32));
    
    metadata.vertexCount = vertexCount;
    metadata.indexCount = indexCount;
    metadata.submeshCount = submeshCount;
    
    // Read bounds
    ReadVec3(file, metadata.bounds.min);
    ReadVec3(file, metadata.bounds.max);
    ReadVec3(file, metadata.bounds.center);
    file.read(reinterpret_cast<char*>(&metadata.bounds.radius), sizeof(f32));
    
    if (!file.good()) return std::nullopt;
    
    return metadata;
}

}  // namespace LumaMeshIO

}  // namespace Luma
