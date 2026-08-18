#include "Luma/Renderer/PrimitiveSceneInfo.h"

namespace Luma {
namespace Renderer2 {

// ============================================================================
// Primitive Scene Info
// ============================================================================

PrimitiveSceneInfo::PrimitiveSceneInfo()
    : m_type(EPrimitiveType::StaticMesh)
    , m_flags(EPrimitiveFlags::Visible | EPrimitiveFlags::CastShadow | EPrimitiveFlags::ReceiveShadow)
    , m_worldTransform(Mat4::Identity()) {
}

PrimitiveSceneInfo::~PrimitiveSceneInfo() {
    ClearMeshBatches();
}

void PrimitiveSceneInfo::AddMeshBatch(StaticMeshBatch* batch) {
    if (batch) {
        m_meshBatches.push_back(batch);
    }
}

void PrimitiveSceneInfo::ClearMeshBatches() {
    m_meshBatches.clear();
}

void PrimitiveSceneInfo::UpdateBounds() {
    // Update world bounds from transform
    // TODO: Implement bounds calculation
    m_worldBounds.center = Vec3(0.0f, 0.0f, 0.0f);
    m_worldBounds.extents = Vec3(1.0f, 1.0f, 1.0f);
    m_worldBounds.min = Vec3(-1.0f, -1.0f, -1.0f);
    m_worldBounds.max = Vec3(1.0f, 1.0f, 1.0f);
}

// ============================================================================
// Static Mesh Scene Info
// ============================================================================

StaticMeshSceneInfo::StaticMeshSceneInfo() {
    m_type = EPrimitiveType::StaticMesh;
}

StaticMeshSceneInfo::~StaticMeshSceneInfo() {
}

// ============================================================================
// Light Scene Info
// ============================================================================

LightSceneInfo::LightSceneInfo() {
    m_type = EPrimitiveType::Light;
    m_flags = EPrimitiveFlags::None;  // Lights don't cast shadows by default
}

LightSceneInfo::~LightSceneInfo() {
}

} // namespace Renderer2
} // namespace Luma