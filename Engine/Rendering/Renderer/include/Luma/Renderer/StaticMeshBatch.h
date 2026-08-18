#pragma once

#include <string>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"

// Static mesh batch structure. Inspired by UE5's mesh batch system
// but adapted for Luma's simpler architecture. Groups mesh elements
// for efficient rendering.

namespace Luma {
namespace Renderer2 {

using std::string;
using std::vector;
using namespace Math;

// Forward declarations
class Material;

// ============================================================================
// Static Mesh Batch
// ============================================================================

// Mesh element
struct MeshElement {
    u32 indexOffset = 0;
    u32 indexCount = 0;
    u32 vertexOffset = 0;
    u32 vertexCount = 0;
    Material* material = nullptr;
};

// Static mesh batch
class StaticMeshBatch {
public:
    StaticMeshBatch();
    ~StaticMeshBatch();
    
    // Get mesh asset ID
    u64 GetMeshAssetId() const { return m_meshAssetId; }
    
    // Set mesh asset ID
    void SetMeshAssetId(u64 assetId) { m_meshAssetId = assetId; }
    
    // Get world transform
    const Mat4& GetWorldTransform() const { return m_worldTransform; }
    
    // Set world transform
    void SetWorldTransform(const Mat4& transform) { m_worldTransform = transform; }
    
    // Get material
    Material* GetMaterial() const { return m_material; }
    
    // Set material
    void SetMaterial(Material* material) { m_material = material; }
    
    // Get mesh elements
    const vector<MeshElement>& GetMeshElements() const { return m_meshElements; }
    
    // Add mesh element
    void AddMeshElement(const MeshElement& element);
    
    // Clear mesh elements
    void ClearMeshElements();
    
    // Get element count
    u32 GetElementCount() const { return static_cast<u32>(m_meshElements.size()); }
    
    // Get index count
    u32 GetIndexCount() const { return m_indexCount; }
    
    // Set index count
    void SetIndexCount(u32 count) { m_indexCount = count; }
    
    // Get vertex count
    u32 GetVertexCount() const { return m_vertexCount; }
    
    // Set vertex count
    void SetVertexCount(u32 count) { m_vertexCount = count; }
    
    // Check if batch is valid
    bool IsValid() const { return m_meshAssetId != 0 && m_material != nullptr; }
    
private:
    u64 m_meshAssetId = 0;
    Mat4 m_worldTransform = Mat4::Identity();
    Material* m_material = nullptr;
    vector<MeshElement> m_meshElements;
    u32 m_indexCount = 0;
    u32 m_vertexCount = 0;
};

// ============================================================================
// Mesh Batch Builder
// ============================================================================

// Builder for creating mesh batches
class MeshBatchBuilder {
public:
    MeshBatchBuilder();
    ~MeshBatchBuilder();
    
    // Begin new batch
    void BeginBatch(u64 meshAssetId);
    
    // End current batch
    StaticMeshBatch* EndBatch();
    
    // Set world transform
    void SetWorldTransform(const Mat4& transform);
    
    // Set material
    void SetMaterial(Material* material);
    
    // Add mesh element
    void AddMeshElement(u32 indexOffset, u32 indexCount, u32 vertexOffset, u32 vertexCount);
    
    // Set counts
    void SetCounts(u32 indexCount, u32 vertexCount);
    
    // Get current batch
    StaticMeshBatch* GetCurrentBatch() const { return m_currentBatch; }
    
private:
    StaticMeshBatch* m_currentBatch = nullptr;
};

} // namespace Renderer2
} // namespace Luma