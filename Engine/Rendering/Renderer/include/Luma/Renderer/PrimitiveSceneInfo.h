#pragma once

#include <string>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"

// Primitive scene information. Inspired by UE5's primitive system
// but adapted for Luma's simpler architecture. Provides the bridge
// between scene objects and renderable primitives.

namespace Luma {
namespace Renderer2 {

using std::string;
using std::vector;
using namespace Math;

// Simple AABB bounds structure
struct AABB {
    Vec3 center;
    Vec3 extents;
    Vec3 min;
    Vec3 max;
    
    AABB() : center(0.0f, 0.0f, 0.0f), extents(1.0f, 1.0f, 1.0f), min(-1.0f, -1.0f, -1.0f), max(1.0f, 1.0f, 1.0f) {}
};

// Forward declarations
class StaticMeshBatch;

// ============================================================================
// Primitive Scene Info
// ============================================================================

// Primitive type
enum class EPrimitiveType : u32 {
    StaticMesh,
    SkeletalMesh,
    ParticleSystem,
    Decal,
    Light,
    Volume,
};

// Primitive flags
enum class EPrimitiveFlags : u32 {
    None = 0,
    CastShadow = 1 << 0,
    ReceiveShadow = 1 << 1,
    Static = 1 << 2,
    Movable = 1 << 3,
    Visible = 1 << 4,
    Translucent = 1 << 5,
};
inline EPrimitiveFlags operator|(EPrimitiveFlags a, EPrimitiveFlags b) {
    return static_cast<EPrimitiveFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline EPrimitiveFlags operator&(EPrimitiveFlags a, EPrimitiveFlags b) {
    return static_cast<EPrimitiveFlags>(static_cast<u32>(a) & static_cast<u32>(b));
}

// Primitive scene info
class PrimitiveSceneInfo {
public:
    PrimitiveSceneInfo();
    virtual ~PrimitiveSceneInfo();
    
    // Get primitive type
    EPrimitiveType GetType() const { return m_type; }
    
    // Get primitive flags
    EPrimitiveFlags GetFlags() const { return m_flags; }
    
    // Set primitive flags
    void SetFlags(EPrimitiveFlags flags) { m_flags = flags; }
    
    // Get world transform
    const Mat4& GetWorldTransform() const { return m_worldTransform; }
    
    // Set world transform
    void SetWorldTransform(const Mat4& transform) { m_worldTransform = transform; }
    
    // Get world bounds
    const AABB& GetWorldBounds() const { return m_worldBounds; }
    
    // Set world bounds
    void SetWorldBounds(const AABB& bounds) { m_worldBounds = bounds; }
    
    // Get primitive name
    const string& GetName() const { return m_name; }
    
    // Set primitive name
    void SetName(const string& name) { m_name = name; }
    
    // Check if primitive casts shadows
    bool CastsShadow() const { return (m_flags & EPrimitiveFlags::CastShadow) != EPrimitiveFlags::None; }
    
    // Check if primitive receives shadows
    bool ReceivesShadow() const { return (m_flags & EPrimitiveFlags::ReceiveShadow) != EPrimitiveFlags::None; }
    
    // Check if primitive is visible
    bool IsVisible() const { return (m_flags & EPrimitiveFlags::Visible) != EPrimitiveFlags::None; }
    
    // Check if primitive is static
    bool IsStatic() const { return (m_flags & EPrimitiveFlags::Static) != EPrimitiveFlags::None; }
    
    // Check if primitive is translucent
    bool IsTranslucent() const { return (m_flags & EPrimitiveFlags::Translucent) != EPrimitiveFlags::None; }
    
    // Get mesh batches (for static mesh primitives)
    virtual const vector<StaticMeshBatch*>& GetMeshBatches() const { return m_meshBatches; }
    
    // Add mesh batch
    void AddMeshBatch(StaticMeshBatch* batch);
    
    // Clear mesh batches
    void ClearMeshBatches();
    
    // Update world bounds from transform
    virtual void UpdateBounds();
    
protected:
    EPrimitiveType m_type;
    EPrimitiveFlags m_flags;
    Mat4 m_worldTransform;
    AABB m_worldBounds;
    string m_name;
    vector<StaticMeshBatch*> m_meshBatches;
};

// ============================================================================
// Static Mesh Scene Info
// ============================================================================

// Static mesh primitive scene info
class StaticMeshSceneInfo : public PrimitiveSceneInfo {
public:
    StaticMeshSceneInfo();
    ~StaticMeshSceneInfo() override;
    
    // Get mesh batches
    const vector<StaticMeshBatch*>& GetMeshBatches() const override { return m_meshBatches; }
    
    // Set mesh asset reference
    void SetMeshAsset(u64 meshAssetId) { m_meshAssetId = meshAssetId; }
    
    // Get mesh asset reference
    u64 GetMeshAssetId() const { return m_meshAssetId; }
    
private:
    u64 m_meshAssetId = 0;
};

// ============================================================================
// Light Scene Info
// ============================================================================

// Light type
enum class ELightType : u32 {
    Directional,
    Point,
    Spot,
    Rect,
};

// Light scene info
class LightSceneInfo : public PrimitiveSceneInfo {
public:
    LightSceneInfo();
    ~LightSceneInfo() override;
    
    // Get light type
    ELightType GetLightType() const { return m_lightType; }
    
    // Set light type
    void SetLightType(ELightType type) { m_lightType = type; }
    
    // Get light color
    const Vec3& GetLightColor() const { return m_lightColor; }
    
    // Set light color
    void SetLightColor(const Vec3& color) { m_lightColor = color; }
    
    // Get light intensity
    f32 GetLightIntensity() const { return m_lightIntensity; }
    
    // Set light intensity
    void SetLightIntensity(f32 intensity) { m_lightIntensity = intensity; }
    
    // Get light radius (for point/spot lights)
    f32 GetLightRadius() const { return m_lightRadius; }
    
    // Set light radius
    void SetLightRadius(f32 radius) { m_lightRadius = radius; }
    
    // Get light direction (for directional/spot lights)
    const Vec3& GetLightDirection() const { return m_lightDirection; }
    
    // Set light direction
    void SetLightDirection(const Vec3& direction) { m_lightDirection = direction; }
    
    // Get spotlight inner angle
    f32 GetSpotInnerAngle() const { return m_spotInnerAngle; }
    
    // Set spotlight inner angle
    void SetSpotInnerAngle(f32 angle) { m_spotInnerAngle = angle; }
    
    // Get spotlight outer angle
    f32 GetSpotOuterAngle() const { return m_spotOuterAngle; }
    
    // Set spotlight outer angle
    void SetSpotOuterAngle(f32 angle) { m_spotOuterAngle = angle; }
    
private:
    ELightType m_lightType = ELightType::Point;
    Vec3 m_lightColor = Vec3(1.0f, 1.0f, 1.0f);
    f32 m_lightIntensity = 1.0f;
    f32 m_lightRadius = 100.0f;
    Vec3 m_lightDirection = Vec3(0.0f, -1.0f, 0.0f);
    f32 m_spotInnerAngle = 30.0f;
    f32 m_spotOuterAngle = 45.0f;
};

} // namespace Renderer2
} // namespace Luma