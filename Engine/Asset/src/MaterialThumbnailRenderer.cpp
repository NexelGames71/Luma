#include "Luma/Asset/MaterialThumbnailRenderer.h"
#include "Luma/Core/Log.h"
#include "Luma/Math/Math.h"
#include "Luma/Math/MathUtils.h"
#include "Luma/Serialization/Json.h"

#include <fstream>
#include <algorithm>
#include <cmath>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4505)
#endif
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace Luma {

MaterialThumbnailRenderer::MaterialThumbnailRenderer() = default;

bool MaterialThumbnailRenderer::CanRender(const AssetId& assetId, const std::filesystem::path& nativePath) const {
    (void)assetId;
    if (!std::filesystem::exists(nativePath)) {
        return false;
    }
    std::string ext = nativePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".lmat" || ext == ".lumat" || ext == ".mat");
}

void MaterialThumbnailRenderer::GetPreferredSize(u32& outWidth, u32& outHeight) const {
    outWidth = 128;
    outHeight = 128;
}

bool MaterialThumbnailRenderer::ParseMaterialFile(const std::filesystem::path& path, MaterialThumbnailData& outMaterial) {
    outMaterial.name = path.stem().string();
    outMaterial.baseColor = Math::Vec3(0.8f, 0.8f, 0.8f);
    outMaterial.roughness = 0.5f;
    outMaterial.metallic = 0.0f;
    outMaterial.specular = 0.5f;
    outMaterial.emissive = Math::Vec3(0.0f, 0.0f, 0.0f);

    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // Try parsing as JSON first
    auto jsonVal = ParseJson(content);
    if (jsonVal && jsonVal->IsObject()) {
        if (auto* name = jsonVal->Find("name"); name && name->IsString()) {
            outMaterial.name = name->AsString();
        }
        // Read a Vec3 array (or leave the fallback value untouched).
        auto readVec3 = [&outMaterial](const SerialValue* v,
                                       Math::Vec3& dst) {
            if (v && v->IsArray() && v->Size() >= 3) {
                dst.x = static_cast<f32>(v->At(0).AsFloat(dst.x));
                dst.y = static_cast<f32>(v->At(1).AsFloat(dst.y));
                dst.z = static_cast<f32>(v->At(2).AsFloat(dst.z));
            }
        };
        // Real .lmat layout (MaterialSerializer): constant fallbacks live
        // under "constants". Material-level base color / metallic /
        // roughness / emissive drive the preview sphere.
        if (auto* constants = jsonVal->Find("constants");
            constants && constants->IsObject()) {
            readVec3(constants->Find("baseColor"), outMaterial.baseColor);
            readVec3(constants->Find("emissive"), outMaterial.emissive);
            if (auto* r = constants->Find("roughness"); r && r->IsNumber()) {
                outMaterial.roughness = static_cast<f32>(r->AsFloat(0.5));
            }
            if (auto* m = constants->Find("metallic"); m && m->IsNumber()) {
                outMaterial.metallic = static_cast<f32>(m->AsFloat(0.0));
            }
        }
        // Legacy / hand-written materials may keep these at the top level.
        readVec3(jsonVal->Find("baseColor"), outMaterial.baseColor);
        readVec3(jsonVal->Find("color"), outMaterial.baseColor);
        if (auto* r = jsonVal->Find("roughness"); r && r->IsNumber()) {
            outMaterial.roughness = static_cast<f32>(r->AsFloat(0.5));
        }
        if (auto* m = jsonVal->Find("metallic"); m && m->IsNumber()) {
            outMaterial.metallic = static_cast<f32>(m->AsFloat(0.0));
        }
        if (auto* s = jsonVal->Find("specular"); s && s->IsNumber()) {
            outMaterial.specular = static_cast<f32>(s->AsFloat(0.5));
        }
        if (auto* tex = jsonVal->Find("albedoTexture"); tex && tex->IsString()) {
            outMaterial.albedoTexturePath = tex->AsString();
        } else if (auto* tex2 = jsonVal->Find("texture"); tex2 && tex2->IsString()) {
            outMaterial.albedoTexturePath = tex2->AsString();
        }
    } else {
        // Line by line key=value fallback
        std::stringstream ss(content);
        std::string line;
        while (std::getline(ss, line)) {
            size_t eq = line.find('=');
            if (eq == std::string::npos) eq = line.find(':');
            if (eq == std::string::npos) continue;

            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            key.erase(0, key.find_first_not_of(" \t\r\n\""));
            key.erase(key.find_last_not_of(" \t\r\n\"") + 1);
            val.erase(0, val.find_first_not_of(" \t\r\n\""));
            val.erase(val.find_last_not_of(" \t\r\n\"") + 1);
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);

            if (key == "roughness") {
                outMaterial.roughness = static_cast<f32>(std::atof(val.c_str()));
            } else if (key == "metallic") {
                outMaterial.metallic = static_cast<f32>(std::atof(val.c_str()));
            } else if (key == "specular") {
                outMaterial.specular = static_cast<f32>(std::atof(val.c_str()));
            } else if (key == "albedotexture" || key == "diffusetexture" || key == "texture") {
                outMaterial.albedoTexturePath = val;
            }
        }
    }

    // Try loading texture if path is provided
    auto dir = path.parent_path();
    if (!outMaterial.albedoTexturePath.empty()) {
        std::vector<std::filesystem::path> candidates = {
            dir / outMaterial.albedoTexturePath,
            dir / "Textures" / outMaterial.albedoTexturePath,
            dir / "textures" / outMaterial.albedoTexturePath,
            outMaterial.albedoTexturePath
        };
        for (const auto& c : candidates) {
            if (std::filesystem::exists(c)) {
                int w = 0, h = 0, ch = 0;
                unsigned char* data = stbi_load(c.string().c_str(), &w, &h, &ch, 4);
                if (data && w > 0 && h > 0) {
                    outMaterial.albedoTexture.width = static_cast<u32>(w);
                    outMaterial.albedoTexture.height = static_cast<u32>(h);
                    outMaterial.albedoTexture.pixels.assign(data, data + w * h * 4);
                    stbi_image_free(data);
                    break;
                }
            }
        }
    }

    return true;
}

void MaterialThumbnailRenderer::GenerateSphereMesh(LumaMeshData& outMesh, u32 segments, u32 rings) {
    outMesh.name = "MaterialPreviewSphere";
    outMesh.vertices.clear();
    outMesh.indices.clear();

    const f32 PI = 3.14159265359f;
    const f32 radius = 1.0f;

    for (u32 r = 0; r <= rings; ++r) {
        f32 theta = static_cast<f32>(r) * PI / static_cast<f32>(rings);
        f32 sinTheta = std::sin(theta);
        f32 cosTheta = std::cos(theta);

        for (u32 s = 0; s <= segments; ++s) {
            f32 phi = static_cast<f32>(s) * 2.0f * PI / static_cast<f32>(segments);
            f32 sinPhi = std::sin(phi);
            f32 cosPhi = std::cos(phi);

            Math::Vec3 normal(cosPhi * sinTheta, cosTheta, sinPhi * sinTheta);
            Math::Vec3 position = normal * radius;
            Math::Vec2 uv(static_cast<f32>(s) / static_cast<f32>(segments),
                          static_cast<f32>(r) / static_cast<f32>(rings));

            LumaVertex vert{};
            vert.position = position;
            vert.normal = normal;
            vert.texCoord = uv;
            outMesh.vertices.push_back(vert);
        }
    }

    for (u32 r = 0; r < rings; ++r) {
        for (u32 s = 0; s < segments; ++s) {
            u32 i0 = r * (segments + 1) + s;
            u32 i1 = (r + 1) * (segments + 1) + s;
            u32 i2 = (r + 1) * (segments + 1) + (s + 1);
            u32 i3 = r * (segments + 1) + (s + 1);

            outMesh.indices.push_back(i0);
            outMesh.indices.push_back(i1);
            outMesh.indices.push_back(i2);

            outMesh.indices.push_back(i0);
            outMesh.indices.push_back(i2);
            outMesh.indices.push_back(i3);
        }
    }

    outMesh.bounds.min = Math::Vec3(-1.0f, -1.0f, -1.0f);
    outMesh.bounds.max = Math::Vec3(1.0f, 1.0f, 1.0f);
    outMesh.bounds.center = Math::Vec3(0.0f, 0.0f, 0.0f);
    outMesh.bounds.radius = 1.0f;
}

bool MaterialThumbnailRenderer::RenderPreviewSphere(
    const MaterialThumbnailData& mat, std::vector<u8>& outPixels,
    u32 width, u32 height, f32 yawDeg, f32 pitchDeg) {
    LumaMeshData sphere;
    GenerateSphereMesh(sphere, 32, 16);
    return RenderMaterialSphere(sphere, mat, outPixels, width, height,
                                yawDeg, pitchDeg);
}

bool MaterialThumbnailRenderer::RenderMaterialSphere(
    const LumaMeshData& sphere, const MaterialThumbnailData& mat,
    std::vector<u8>& outPixels, u32 width, u32 height, f32 yawDeg,
    f32 pitchDeg) {
    outPixels.resize(width * height * 4);
    std::vector<f32> depthBuffer(width * height, 1e9f);

    // Studio dark circular gradient backdrop
    f32 cx = static_cast<f32>(width) * 0.5f;
    f32 cy = static_cast<f32>(height) * 0.5f;
    f32 maxDist = std::sqrt(cx * cx + cy * cy);

    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            f32 dx = static_cast<f32>(x) - cx;
            f32 dy = static_cast<f32>(y) - cy;
            f32 d = std::sqrt(dx * dx + dy * dy) / maxDist;
            u8 r = static_cast<u8>(std::clamp(38.0f * (1.0f - d) + 20.0f * d, 0.0f, 255.0f));
            u8 g = static_cast<u8>(std::clamp(42.0f * (1.0f - d) + 22.0f * d, 0.0f, 255.0f));
            u8 b = static_cast<u8>(std::clamp(50.0f * (1.0f - d) + 26.0f * d, 0.0f, 255.0f));
            u32 idx = (y * width + x) * 4;
            outPixels[idx + 0] = r;
            outPixels[idx + 1] = g;
            outPixels[idx + 2] = b;
            outPixels[idx + 3] = 255;
        }
    }

    // Camera view setup
    Math::Vec3 eye(0.0f, 0.35f, 2.6f);
    Math::Vec3 center(0.0f, 0.0f, 0.0f);
    Math::Vec3 up(0.0f, 1.0f, 0.0f);

    // Orbit the camera around the sphere (Material Editor preview viewport).
    // Rotate the eye around the center by yaw (around Y) then pitch (around
    // X), keeping the same viewing distance.
    if (yawDeg != 0.0f || pitchDeg != 0.0f) {
        f32 yaw = Math::Radians(yawDeg);
        f32 pitch = Math::Radians(pitchDeg);
        Math::Vec3 off = eye - center;
        // Yaw around the world Y axis.
        f32 cosY = std::cos(yaw), sinY = std::sin(yaw);
        Math::Vec3 ey{off.x * cosY + off.z * sinY, off.y,
                      -off.x * sinY + off.z * cosY};
        // Pitch around the local X axis.
        f32 cosP = std::cos(pitch), sinP = std::sin(pitch);
        Math::Vec3 ep{ey.x, ey.y * cosP - ey.z * sinP,
                      ey.y * sinP + ey.z * cosP};
        eye = center + ep;
    }

    Math::Vec3 f = Math::Normalize(center - eye);
    Math::Vec3 s = Math::Normalize(Math::Cross(f, up));
    Math::Vec3 u = Math::Cross(s, f);

    Math::Mat4 viewMatrix = Math::Mat4::Identity();
    viewMatrix.m[0] = s.x; viewMatrix.m[4] = s.y; viewMatrix.m[8] = s.z;
    viewMatrix.m[1] = u.x; viewMatrix.m[5] = u.y; viewMatrix.m[9] = u.z;
    viewMatrix.m[2] = -f.x; viewMatrix.m[6] = -f.y; viewMatrix.m[10] = -f.z;
    viewMatrix.m[12] = -Math::Dot(s, eye);
    viewMatrix.m[13] = -Math::Dot(u, eye);
    viewMatrix.m[14] = Math::Dot(f, eye);

    f32 orthoSize = 2.3f;
    f32 aspect = static_cast<f32>(width) / static_cast<f32>(height);

    Math::Mat4 projMatrix = Math::Mat4::Identity();
    projMatrix.m[0] = 2.0f / (orthoSize * aspect);
    projMatrix.m[5] = 2.0f / orthoSize;
    projMatrix.m[10] = -0.5f;
    projMatrix.m[15] = 1.0f;

    Math::Mat4 mvp = projMatrix * viewMatrix;

    struct ScreenVert {
        f32 x, y, z;
        Math::Vec3 normal;
        Math::Vec2 uv;
    };

    std::vector<ScreenVert> screenVerts(sphere.vertices.size());
    for (size_t i = 0; i < sphere.vertices.size(); ++i) {
        const auto& v = sphere.vertices[i];
        f32 x = v.position.x;
        f32 y = v.position.y;
        f32 z = v.position.z;
        f32 w = 1.0f;

        f32 tx = mvp.m[0] * x + mvp.m[4] * y + mvp.m[8] * z + mvp.m[12] * w;
        f32 ty = mvp.m[1] * x + mvp.m[5] * y + mvp.m[9] * z + mvp.m[13] * w;
        f32 tz = mvp.m[2] * x + mvp.m[6] * y + mvp.m[10] * z + mvp.m[14] * w;
        f32 tw = mvp.m[3] * x + mvp.m[7] * y + mvp.m[11] * z + mvp.m[15] * w;

        if (tw != 0.0f) {
            tx /= tw;
            ty /= tw;
            tz /= tw;
        }

        screenVerts[i].x = (tx + 1.0f) * 0.5f * static_cast<f32>(width);
        screenVerts[i].y = (1.0f - ty) * 0.5f * static_cast<f32>(height);
        screenVerts[i].z = tz;
        screenVerts[i].normal = v.normal;
        screenVerts[i].uv = v.texCoord;
    }

    Math::Vec3 lightDir1 = Math::Normalize(Math::Vec3(0.5f, 0.8f, 0.9f));
    Math::Vec3 lightDir2 = Math::Normalize(Math::Vec3(-0.7f, -0.2f, 0.4f));

    auto sampleTexture = [&](f32 u, f32 v) -> Math::Vec3 {
        if (!mat.albedoTexture.IsValid()) {
            return mat.baseColor;
        }
        u = u - std::floor(u);
        v = v - std::floor(v);
        f32 tx = u * static_cast<f32>(mat.albedoTexture.width - 1);
        f32 ty = (1.0f - v) * static_cast<f32>(mat.albedoTexture.height - 1);
        u32 px = std::clamp(static_cast<u32>(tx + 0.5f), 0u, mat.albedoTexture.width - 1);
        u32 py = std::clamp(static_cast<u32>(ty + 0.5f), 0u, mat.albedoTexture.height - 1);
        u32 idx = (py * mat.albedoTexture.width + px) * 4;
        return Math::Vec3{
            static_cast<f32>(mat.albedoTexture.pixels[idx + 0]) / 255.0f,
            static_cast<f32>(mat.albedoTexture.pixels[idx + 1]) / 255.0f,
            static_cast<f32>(mat.albedoTexture.pixels[idx + 2]) / 255.0f
        };
    };

    for (size_t i = 0; i < sphere.indices.size(); i += 3) {
        u32 i0 = sphere.indices[i];
        u32 i1 = sphere.indices[i + 1];
        u32 i2 = sphere.indices[i + 2];
        if (i0 >= screenVerts.size() || i1 >= screenVerts.size() || i2 >= screenVerts.size()) continue;

        const auto& v0 = screenVerts[i0];
        const auto& v1 = screenVerts[i1];
        const auto& v2 = screenVerts[i2];

        f32 area = (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x);
        if (area <= 0.0001f) continue;
        f32 invArea = 1.0f / area;

        i32 minX = std::max(0, static_cast<i32>(std::floor(std::min({v0.x, v1.x, v2.x}))));
        i32 maxX = std::min(static_cast<i32>(width) - 1, static_cast<i32>(std::ceil(std::max({v0.x, v1.x, v2.x}))));
        i32 minY = std::max(0, static_cast<i32>(std::floor(std::min({v0.y, v1.y, v2.y}))));
        i32 maxY = std::min(static_cast<i32>(height) - 1, static_cast<i32>(std::ceil(std::max({v0.y, v1.y, v2.y}))));

        for (i32 py = minY; py <= maxY; ++py) {
            for (i32 px = minX; px <= maxX; ++px) {
                f32 fx = static_cast<f32>(px) + 0.5f;
                f32 fy = static_cast<f32>(py) + 0.5f;

                f32 w0 = ((v1.x - fx) * (v2.y - fy) - (v1.y - fy) * (v2.x - fx)) * invArea;
                f32 w1 = ((v2.x - fx) * (v0.y - fy) - (v2.y - fy) * (v0.x - fx)) * invArea;
                f32 w2 = 1.0f - w0 - w1;

                if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                    f32 z = w0 * v0.z + w1 * v1.z + w2 * v2.z;
                    u32 pidx = py * width + px;
                    if (z < depthBuffer[pidx]) {
                        depthBuffer[pidx] = z;

                        Math::Vec3 norm = Math::Normalize(v0.normal * w0 + v1.normal * w1 + v2.normal * w2);
                        Math::Vec2 uv = v0.uv * w0 + v1.uv * w1 + v2.uv * w2;
                        Math::Vec3 albedo = sampleTexture(uv.x, uv.y);

                        // Diffuse lighting
                        f32 diff1 = std::max(0.0f, Math::Dot(norm, lightDir1));
                        f32 diff2 = std::max(0.0f, Math::Dot(norm, lightDir2)) * 0.30f;
                        f32 ambient = 0.25f;

                        // Specular highlight based on roughness
                        Math::Vec3 viewDir = Math::Normalize(eye - center);
                        Math::Vec3 halfDir = Math::Normalize(lightDir1 + viewDir);
                        f32 specPower = std::max(2.0f, (1.0f - mat.roughness) * 128.0f);
                        f32 spec = std::pow(std::max(0.0f, Math::Dot(norm, halfDir)), specPower) * (1.0f - mat.roughness);

                        // Rim reflection
                        f32 rim = std::pow(1.0f - std::max(0.0f, Math::Dot(norm, viewDir)), 3.0f) * 0.25f;

                        // Metal-aware preview: metals reflect the base color in
                        // the specular term and have little diffuse response.
                        f32 diffuseIntensity = diff1 * 0.70f + diff2 + ambient + rim;
                        Math::Vec3 specColor =
                            albedo * (spec * mat.metallic) +
                            Math::Vec3(spec, spec, spec) * (1.0f - mat.metallic);
                        Math::Vec3 finalColor =
                            albedo * diffuseIntensity * (1.0f - mat.metallic * 0.9f) +
                            specColor + mat.emissive;

                        outPixels[pidx * 4 + 0] = static_cast<u8>(std::clamp(finalColor.x * 255.0f, 0.0f, 255.0f));
                        outPixels[pidx * 4 + 1] = static_cast<u8>(std::clamp(finalColor.y * 255.0f, 0.0f, 255.0f));
                        outPixels[pidx * 4 + 2] = static_cast<u8>(std::clamp(finalColor.z * 255.0f, 0.0f, 255.0f));
                        outPixels[pidx * 4 + 3] = 255;
                    }
                }
            }
        }
    }

    return true;
}

bool MaterialThumbnailRenderer::RenderThumbnail(const AssetId& assetId,
                                                const std::filesystem::path& nativePath,
                                                const std::filesystem::path& outputPath,
                                                u32 width,
                                                u32 height) {
    (void)assetId;

    MaterialThumbnailData matData;
    if (!ParseMaterialFile(nativePath, matData)) {
        LUMA_LOG_ERROR("MaterialThumbnailRenderer", "Failed to parse material file: {}", nativePath.string());
        return false;
    }

    LumaMeshData sphere;
    GenerateSphereMesh(sphere, 32, 16);

    std::vector<u8> pixels;
    if (!RenderMaterialSphere(sphere, matData, pixels, width, height)) {
        LUMA_LOG_ERROR("MaterialThumbnailRenderer", "Failed to render material preview sphere");
        return false;
    }

    int success = stbi_write_png(outputPath.string().c_str(),
                                  static_cast<int>(width),
                                  static_cast<int>(height),
                                  4,
                                  pixels.data(),
                                  static_cast<int>(width * 4));

    if (!success) {
        LUMA_LOG_ERROR("MaterialThumbnailRenderer", "Failed to save thumbnail: {}", outputPath.string());
        return false;
    }

    LUMA_LOG_INFO("MaterialThumbnailRenderer", "Generated material thumbnail: {}", outputPath.string());
    return true;
}

} // namespace Luma
