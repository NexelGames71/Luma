#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"
#include "Luma/Shader/ShaderParameterStruct.h"

// Material system. Inspired by UE5's material system but adapted
// for Luma's simpler architecture. Provides material parameter
// management and shader binding.

namespace Luma {
namespace Renderer2 {

using std::string;
using std::vector;
using std::unordered_map;
using namespace Math;

// Forward declarations
class MaterialShader;

// ============================================================================
// Material Parameter
// ============================================================================

// Material parameter type
enum class EMaterialParameterType : u32 {
    Scalar,
    Vector2,
    Vector3,
    Vector4,
    Color,
    Texture,
    NormalMap,
    Roughness,
    Metallic,
    Specular,
};

// Material parameter
struct MaterialParameter {
    string name;
    EMaterialParameterType type;
    union {
        f32 scalar;
        Vec2 vector2;
        Vec3 vector3;
        Vec4 vector4;
    };
    u64 textureHandle = 0;
    
    MaterialParameter() : type(EMaterialParameterType::Scalar), scalar(0.0f) {
        vector2 = Vec2(0.0f, 0.0f);
        vector3 = Vec3(0.0f, 0.0f, 0.0f);
        vector4 = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }
};

// ============================================================================
// Material
// ============================================================================

// Material blend mode
enum class EMaterialBlendMode : u32 {
    Opaque,
    Masked,
    Translucent,
    Additive,
    Modulate,
};

// Material shading model
enum class EMaterialShadingModel : u32 {
    Unlit,
    DefaultLit,
    Subsurface,
    ClearCoat,
    Cloth,
    Eye,
};

// Material class
class Material {
public:
    Material();
    ~Material();
    
    // Get material name
    const string& GetName() const { return m_name; }
    
    // Set material name
    void SetName(const string& name) { m_name = name; }
    
    // Get blend mode
    EMaterialBlendMode GetBlendMode() const { return m_blendMode; }
    
    // Set blend mode
    void SetBlendMode(EMaterialBlendMode mode) { m_blendMode = mode; }
    
    // Get shading model
    EMaterialShadingModel GetShadingModel() const { return m_shadingModel; }
    
    // Set shading model
    void SetShadingModel(EMaterialShadingModel model) { m_shadingModel = model; }
    
    // Get opacity
    f32 GetOpacity() const { return m_opacity; }
    
    // Set opacity
    void SetOpacity(f32 opacity) { m_opacity = opacity; }
    
    // Get opacity mask
    f32 GetOpacityMask() const { return m_opacityMask; }
    
    // Set opacity mask
    void SetOpacityMask(f32 mask) { m_opacityMask = mask; }
    
    // Get roughness
    f32 GetRoughness() const { return m_roughness; }
    
    // Set roughness
    void SetRoughness(f32 roughness) { m_roughness = roughness; }
    
    // Get metallic
    f32 GetMetallic() const { return m_metallic; }
    
    // Set metallic
    void SetMetallic(f32 metallic) { m_metallic = metallic; }
    
    // Get specular
    f32 GetSpecular() const { return m_specular; }
    
    // Set specular
    void SetSpecular(f32 specular) { m_specular = specular; }
    
    // Get base color
    const Vec3& GetBaseColor() const { return m_baseColor; }
    
    // Set base color
    void SetBaseColor(const Vec3& color) { m_baseColor = color; }
    
    // Get emissive color
    const Vec3& GetEmissiveColor() const { return m_emissiveColor; }
    
    // Set emissive color
    void SetEmissiveColor(const Vec3& color) { m_emissiveColor = color; }
    
    // Get emissive intensity
    f32 GetEmissiveIntensity() const { return m_emissiveIntensity; }
    
    // Set emissive intensity
    void SetEmissiveIntensity(f32 intensity) { m_emissiveIntensity = intensity; }
    
    // Get normal map handle
    u64 GetNormalMap() const { return m_normalMap; }
    
    // Set normal map
    void SetNormalMap(u64 textureHandle) { m_normalMap = textureHandle; }
    
    // Get albedo texture handle
    u64 GetAlbedoTexture() const { return m_albedoTexture; }
    
    // Set albedo texture
    void SetAlbedoTexture(u64 textureHandle) { m_albedoTexture = textureHandle; }
    
    // Get roughness texture handle
    u64 GetRoughnessTexture() const { return m_roughnessTexture; }
    
    // Set roughness texture
    void SetRoughnessTexture(u64 textureHandle) { m_roughnessTexture = textureHandle; }
    
    // Get metallic texture handle
    u64 GetMetallicTexture() const { return m_metallicTexture; }
    
    // Set metallic texture
    void SetMetallicTexture(u64 textureHandle) { m_metallicTexture = textureHandle; }
    
    // Get ao texture handle
    u64 GetAOTexture() const { return m_aoTexture; }
    
    // Set ao texture
    void SetAOTexture(u64 textureHandle) { m_aoTexture = textureHandle; }
    
    // Add custom parameter
    void AddParameter(const MaterialParameter& parameter);
    
    // Get custom parameter
    const MaterialParameter* GetParameter(const string& name) const;
    
    // Get all custom parameters
    const vector<MaterialParameter>& GetParameters() const { return m_parameters; }
    
    // Check if material is transparent
    bool IsTransparent() const { return m_blendMode == EMaterialBlendMode::Translucent || m_opacity < 1.0f; }
    
    // Check if material is masked
    bool IsMasked() const { return m_blendMode == EMaterialBlendMode::Masked; }
    
    // Check if material is opaque
    bool IsOpaque() const { return m_blendMode == EMaterialBlendMode::Opaque && m_opacity >= 1.0f; }
    
    // Get material shader
    MaterialShader* GetMaterialShader() const { return m_materialShader; }
    
    // Set material shader
    void SetMaterialShader(MaterialShader* shader) { m_materialShader = shader; }
    
    // Update parameter struct
    void UpdateParameterStruct(Shader::ShaderParameterInstance* instance);
    
private:
    string m_name;
    EMaterialBlendMode m_blendMode = EMaterialBlendMode::Opaque;
    EMaterialShadingModel m_shadingModel = EMaterialShadingModel::DefaultLit;
    f32 m_opacity = 1.0f;
    f32 m_opacityMask = 0.5f;
    f32 m_roughness = 0.5f;
    f32 m_metallic = 0.0f;
    f32 m_specular = 0.5f;
    Vec3 m_baseColor = Vec3(1.0f, 1.0f, 1.0f);
    Vec3 m_emissiveColor = Vec3(0.0f, 0.0f, 0.0f);
    f32 m_emissiveIntensity = 0.0f;
    u64 m_normalMap = 0;
    u64 m_albedoTexture = 0;
    u64 m_roughnessTexture = 0;
    u64 m_metallicTexture = 0;
    u64 m_aoTexture = 0;
    vector<MaterialParameter> m_parameters;
    MaterialShader* m_materialShader = nullptr;
};

// ============================================================================
// Material Registry
// ============================================================================

// Global material registry
class MaterialRegistry {
public:
    static MaterialRegistry& GetInstance();
    
    // Register material
    void RegisterMaterial(Material* material);
    
    // Unregister material
    void UnregisterMaterial(const string& name);
    
    // Get material by name
    Material* GetMaterial(const string& name) const;
    
    // Get all materials
    const vector<Material*>& GetAllMaterials() const { return m_materials; }
    
    // Clear all materials
    void Clear();
    
private:
    MaterialRegistry() = default;
    ~MaterialRegistry();
    
    vector<Material*> m_materials;
    unordered_map<string, Material*> m_materialMap;
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Create default material
inline Material* CreateDefaultMaterial() {
    auto* material = new Material();
    material->SetName("DefaultMaterial");
    material->SetBlendMode(EMaterialBlendMode::Opaque);
    material->SetShadingModel(EMaterialShadingModel::DefaultLit);
    material->SetBaseColor(Vec3(1.0f, 1.0f, 1.0f));
    material->SetRoughness(0.5f);
    material->SetMetallic(0.0f);
    return material;
}

// Register material
inline void RegisterMaterial(Material* material) {
    MaterialRegistry::GetInstance().RegisterMaterial(material);
}

// Get material by name
inline Material* GetMaterial(const string& name) {
    return MaterialRegistry::GetInstance().GetMaterial(name);
}

} // namespace Renderer2
} // namespace Luma