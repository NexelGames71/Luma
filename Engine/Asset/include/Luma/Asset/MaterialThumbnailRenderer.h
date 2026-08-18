#pragma once

#include "Luma/Asset/ThumbnailRenderer.h"
#include "Luma/Asset/LumaMesh.h"
#include "Luma/Asset/MeshThumbnailRenderer.h"
#include "Luma/Core/Types.h"
#include "Luma/Math/Math.h"

#include <filesystem>
#include <vector>
#include <string>

namespace Luma {

struct MaterialThumbnailData {
    std::string name;
    Math::Vec3 baseColor{0.8f, 0.8f, 0.8f};
    f32 roughness = 0.5f;
    f32 metallic = 0.0f;
    f32 specular = 0.5f;
    Math::Vec3 emissive{0.0f, 0.0f, 0.0f};
    std::string albedoTexturePath;
    TexturePreviewData albedoTexture;
};

/**
 * Thumbnail renderer for Material assets (.lmat, .lumat, .mat)
 * Renders a studio-lit material preview sphere
 */
class MaterialThumbnailRenderer : public ThumbnailRenderer {
public:
    MaterialThumbnailRenderer();
    ~MaterialThumbnailRenderer() override = default;
    
    AssetType GetAssetType() const override { return AssetType::Material; }
    
    bool CanRender(const AssetId& assetId, const std::filesystem::path& nativePath) const override;
    
    bool RenderThumbnail(const AssetId& assetId,
                        const std::filesystem::path& nativePath,
                        const std::filesystem::path& outputPath,
                        u32 width,
                        u32 height) override;
    
    void GetPreferredSize(u32& outWidth, u32& outHeight) const override;
    
    std::string GetRendererName() const override { return "MaterialThumbnailRenderer"; }

    // Renders the studio-lit preview sphere for a material into RGBA8
    // pixels. yawDeg/pitchDeg orbit the camera around the sphere (used by
    // the Material Editor's 3D preview viewport). Same lighting / shading
    // as the thumbnail path.
    static bool RenderPreviewSphere(const MaterialThumbnailData& mat,
                                    std::vector<u8>& outPixels,
                                    u32 width,
                                    u32 height,
                                    f32 yawDeg = 0.0f,
                                    f32 pitchDeg = 0.0f);
    
private:
    bool ParseMaterialFile(const std::filesystem::path& path, MaterialThumbnailData& outMaterial);
    static void GenerateSphereMesh(LumaMeshData& outMesh, u32 segments = 32, u32 rings = 16);
    static bool RenderMaterialSphere(const LumaMeshData& sphere,
                                     const MaterialThumbnailData& mat,
                                     std::vector<u8>& outPixels,
                                     u32 width,
                                     u32 height,
                                     f32 yawDeg = 0.0f,
                                     f32 pitchDeg = 0.0f);
};

} // namespace Luma
