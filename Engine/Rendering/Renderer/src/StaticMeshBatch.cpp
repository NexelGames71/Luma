#include "Luma/Renderer/StaticMeshBatch.h"

namespace Luma {
namespace Renderer2 {

// ============================================================================
// Static Mesh Batch
// ============================================================================

StaticMeshBatch::StaticMeshBatch()
    : m_worldTransform(Mat4::Identity()) {
}

StaticMeshBatch::~StaticMeshBatch() {
    ClearMeshElements();
}

void StaticMeshBatch::AddMeshElement(const MeshElement& element) {
    m_meshElements.push_back(element);
}

void StaticMeshBatch::ClearMeshElements() {
    m_meshElements.clear();
}

// ============================================================================
// Mesh Batch Builder
// ============================================================================

MeshBatchBuilder::MeshBatchBuilder() {
}

MeshBatchBuilder::~MeshBatchBuilder() {
    if (m_currentBatch) {
        delete m_currentBatch;
    }
}

void MeshBatchBuilder::BeginBatch(u64 meshAssetId) {
    if (m_currentBatch) {
        delete m_currentBatch;
    }
    
    m_currentBatch = new StaticMeshBatch();
    m_currentBatch->SetMeshAssetId(meshAssetId);
}

StaticMeshBatch* MeshBatchBuilder::EndBatch() {
    auto* batch = m_currentBatch;
    m_currentBatch = nullptr;
    return batch;
}

void MeshBatchBuilder::SetWorldTransform(const Mat4& transform) {
    if (m_currentBatch) {
        m_currentBatch->SetWorldTransform(transform);
    }
}

void MeshBatchBuilder::SetMaterial(Material* material) {
    if (m_currentBatch) {
        m_currentBatch->SetMaterial(material);
    }
}

void MeshBatchBuilder::AddMeshElement(u32 indexOffset, u32 indexCount, u32 vertexOffset, u32 vertexCount) {
    if (m_currentBatch) {
        MeshElement element;
        element.indexOffset = indexOffset;
        element.indexCount = indexCount;
        element.vertexOffset = vertexOffset;
        element.vertexCount = vertexCount;
        m_currentBatch->AddMeshElement(element);
    }
}

void MeshBatchBuilder::SetCounts(u32 indexCount, u32 vertexCount) {
    if (m_currentBatch) {
        m_currentBatch->SetIndexCount(indexCount);
        m_currentBatch->SetVertexCount(vertexCount);
    }
}

} // namespace Renderer2
} // namespace Luma