#include "Luma/Asset/MeshThumbnailRenderer.h"
#include "Luma/Core/Log.h"
#include "Luma/Math/Math.h"
#include "Luma/Math/MathUtils.h"
#include "Luma/Renderer/DeferredShadingRenderer.h"

#include <fstream>
#include <algorithm>
#include <cmath>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>

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

// ============================================================================
// MeshThumbnailRenderer Implementation
// ============================================================================

MeshThumbnailRenderer::MeshThumbnailRenderer()
    : m_device(nullptr), m_deferredRenderer(nullptr), m_useGPU(false) {
}

MeshThumbnailRenderer::~MeshThumbnailRenderer() {
    if (m_deferredRenderer) {
        delete m_deferredRenderer;
    }
}

bool MeshThumbnailRenderer::Initialize(RHI::RHIDevice* device) {
    m_device = device;
    
    if (m_device) {
        // Create deferred renderer for GPU rendering
        m_deferredRenderer = Renderer2::CreateDeferredRenderer();
        if (m_deferredRenderer) {
            if (m_deferredRenderer->Initialize(m_device)) {
                m_useGPU = true;
                LUMA_LOG_INFO("MeshThumbnailRenderer", "GPU rendering enabled");
            } else {
                LUMA_LOG_WARN("MeshThumbnailRenderer", "Failed to initialize deferred renderer, using software fallback");
            }
        }
    }
    
    return true;
}

bool MeshThumbnailRenderer::CanRender(const AssetId& assetId, const std::filesystem::path& nativePath) const {
    (void)assetId;
    
    if (!std::filesystem::exists(nativePath)) {
        return false;
    }
    
    std::string ext = nativePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return (ext == ".lmesh" || ext == ".fbx" || ext == ".obj" || ext == ".gltf" ||
            ext == ".glb" || ext == ".dae" || ext == ".blend" || ext == ".3ds");
}

bool MeshThumbnailRenderer::RenderThumbnail(const AssetId& assetId,
                                            const std::filesystem::path& nativePath,
                                            const std::filesystem::path& outputPath,
                                            u32 width,
                                            u32 height) {
    (void)assetId;
    
    LUMA_LOG_INFO("MeshThumbnailRenderer", "Attempting to render thumbnail for: {} -> {}", nativePath.string(), outputPath.string());
    
    // For now, use software rendering to ensure thumbnails work
    // GPU rendering is not fully implemented yet (texture capture missing)
    // Load mesh and associated texture
    LumaMeshData mesh;
    TexturePreviewData texture;
    if (!LoadMesh(nativePath, mesh, texture)) {
        LUMA_LOG_ERROR("MeshThumbnailRenderer", "Failed to load mesh: {}", nativePath.string());
        return false;
    }
    
    if (!mesh.IsValid()) {
        LUMA_LOG_ERROR("MeshThumbnailRenderer", "Invalid mesh data");
        return false;
    }
    
    LUMA_LOG_INFO("MeshThumbnailRenderer", "Mesh loaded successfully: {} vertices, {} indices", mesh.vertices.size(), mesh.indices.size());
    
    // Render solid mesh preview with texture and lighting
    std::vector<u8> pixels;
    if (!RenderSolid(mesh, texture.IsValid() ? &texture : nullptr, pixels, width, height)) {
        LUMA_LOG_ERROR("MeshThumbnailRenderer", "Failed to render solid mesh preview");
        return false;
    }
    
    // Write PNG
    int success = stbi_write_png(outputPath.string().c_str(),
                                  static_cast<int>(width),
                                  static_cast<int>(height),
                                  4,
                                  pixels.data(),
                                  static_cast<int>(width * 4));
    
    if (!success) {
        LUMA_LOG_ERROR("MeshThumbnailRenderer", "Failed to write thumbnail: {}", outputPath.string());
        return false;
    }
    
    LUMA_LOG_INFO("MeshThumbnailRenderer", "Generated thumbnail: {} vertices (textured: {}) -> {}", 
                  mesh.vertices.size(), texture.IsValid(), outputPath.string());
    
    return true;
}

void MeshThumbnailRenderer::GetPreferredSize(u32& outWidth, u32& outHeight) const {
    outWidth = 128;
    outHeight = 128;
}

bool MeshThumbnailRenderer::RenderGPUThumbnail(const AssetId& assetId,
                                               const std::filesystem::path& nativePath,
                                               const std::filesystem::path& outputPath,
                                               u32 width,
                                               u32 height) {
    (void)assetId;
    (void)outputPath;
    
    // Load mesh
    LumaMeshData mesh;
    if (!LoadMesh(nativePath, mesh)) {
        LUMA_LOG_ERROR("MeshThumbnailRenderer", "Failed to load mesh for GPU rendering: {}", nativePath.string());
        return false;
    }
    
    if (!mesh.IsValid()) {
        LUMA_LOG_ERROR("MeshThumbnailRenderer", "Invalid mesh data for GPU rendering");
        return false;
    }
    
    // Build thumbnail scene view
    Renderer2::DeferredSceneView sceneView = BuildThumbnailSceneView(mesh, width, height);
    
    // Set up deferred renderer for thumbnail rendering
    m_deferredRenderer->SetViewportDimensions(width, height);
    m_deferredRenderer->SetEditorRenderMode(Renderer2::EditorRenderMode::Unlit);
    m_deferredRenderer->SetDebugVisualization(false);
    
    // Prepare and render
    m_deferredRenderer->PrepareScene();
    m_deferredRenderer->RenderScene(sceneView);
    
    // TODO: Capture output from deferred renderer and save to file
    // This requires implementing texture capture from the deferred renderer's output
    LUMA_LOG_INFO("MeshThumbnailRenderer", "GPU rendering stub: {} vertices", mesh.vertices.size());
    
    return false; // TODO: Return true when texture capture is implemented
}

Renderer2::DeferredSceneView MeshThumbnailRenderer::BuildThumbnailSceneView(const LumaMeshData& mesh, u32 width, u32 height) {
    using namespace Math;
    
    Renderer2::DeferredSceneView sceneView;
    
    // Center camera on mesh bounds
    Vec3 center = mesh.bounds.center;
    f32 radius = mesh.bounds.radius;
    
    // Position camera at diagonal
    sceneView.cameraPosition = center + Vec3(radius * 1.5f, radius * 1.5f, radius * 1.5f);
    sceneView.cameraDirection = Normalize(center - sceneView.cameraPosition);
    
    // Set up view matrix (look at center)
    Vec3 up = Vec3(0.0f, 0.0f, 1.0f);
    Vec3 f = Normalize(center - sceneView.cameraPosition);
    Vec3 s = Normalize(Cross(f, up));
    Vec3 u = Cross(s, f);
    
    sceneView.viewMatrix = Mat4::Identity();
    sceneView.viewMatrix.m[0] = s.x; sceneView.viewMatrix.m[4] = s.y; sceneView.viewMatrix.m[8] = s.z;
    sceneView.viewMatrix.m[1] = u.x; sceneView.viewMatrix.m[5] = u.y; sceneView.viewMatrix.m[9] = u.z;
    sceneView.viewMatrix.m[2] = -f.x; sceneView.viewMatrix.m[6] = -f.y; sceneView.viewMatrix.m[10] = -f.z;
    sceneView.viewMatrix.m[12] = -Dot(s, sceneView.cameraPosition);
    sceneView.viewMatrix.m[13] = -Dot(u, sceneView.cameraPosition);
    sceneView.viewMatrix.m[14] = Dot(f, sceneView.cameraPosition);
    
    // Orthographic projection
    f32 orthoSize = radius * 2.0f;
    f32 aspect = static_cast<f32>(width) / static_cast<f32>(height);
    
    sceneView.projectionMatrix = Mat4::Identity();
    sceneView.projectionMatrix.m[0] = 2.0f / (orthoSize * aspect);
    sceneView.projectionMatrix.m[5] = 2.0f / orthoSize;
    sceneView.projectionMatrix.m[10] = -2.0f / (orthoSize * 2.0f);
    sceneView.projectionMatrix.m[15] = 1.0f;
    
    sceneView.viewProjectionMatrix = sceneView.projectionMatrix * sceneView.viewMatrix;
    
    sceneView.fov = 1.0f;
    sceneView.nearPlane = 0.1f;
    sceneView.farPlane = radius * 10.0f;
    sceneView.width = width;
    sceneView.height = height;
    sceneView.time = 0.0f;
    sceneView.deltaTime = 0.0f;
    
    return sceneView;
}

bool MeshThumbnailRenderer::LoadTextureFromFile(const std::filesystem::path& path, TexturePreviewData& outTexture) {
    if (!std::filesystem::exists(path)) return false;
    
    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
    if (!data || w <= 0 || h <= 0) {
        if (data) stbi_image_free(data);
        return false;
    }
    
    outTexture.width = static_cast<u32>(w);
    outTexture.height = static_cast<u32>(h);
    outTexture.pixels.assign(data, data + w * h * 4);
    stbi_image_free(data);
    return true;
}

bool MeshThumbnailRenderer::FindAndLoadTexture(const std::filesystem::path& meshPath,
                                              const void* assimpScenePtr,
                                              TexturePreviewData& outTexture) {
    const aiScene* scene = static_cast<const aiScene*>(assimpScenePtr);
    auto meshDir = meshPath.parent_path();
    
    // 1. Check textures referenced in Assimp materials
    if (scene) {
        for (u32 m = 0; m < scene->mNumMaterials; ++m) {
            const aiMaterial* mat = scene->mMaterials[m];
            
            static const aiTextureType kTextureTypes[] = {
                aiTextureType_DIFFUSE,
                aiTextureType_BASE_COLOR,
                aiTextureType_EMISSIVE,
                aiTextureType_UNKNOWN
            };
            
            for (auto type : kTextureTypes) {
                u32 texCount = mat->GetTextureCount(type);
                for (u32 t = 0; t < texCount; ++t) {
                    aiString texPath;
                    if (mat->GetTexture(type, t, &texPath) == AI_SUCCESS) {
                        std::string rawPath = texPath.C_Str();
                        if (rawPath.empty()) continue;
                        
                        // Embedded texture (*0, *1, etc.)
                        if (rawPath[0] == '*') {
                            int texIndex = std::atoi(rawPath.c_str() + 1);
                            if (texIndex >= 0 && static_cast<u32>(texIndex) < scene->mNumTextures) {
                                const aiTexture* embTex = scene->mTextures[texIndex];
                                if (embTex->mHeight == 0) {
                                    int w = 0, h = 0, channels = 0;
                                    unsigned char* data = stbi_load_from_memory(
                                        reinterpret_cast<const unsigned char*>(embTex->pcData),
                                        static_cast<int>(embTex->mWidth),
                                        &w, &h, &channels, 4
                                    );
                                    if (data && w > 0 && h > 0) {
                                        outTexture.width = static_cast<u32>(w);
                                        outTexture.height = static_cast<u32>(h);
                                        outTexture.pixels.assign(data, data + w * h * 4);
                                        stbi_image_free(data);
                                        return true;
                                    }
                                } else {
                                    outTexture.width = embTex->mWidth;
                                    outTexture.height = embTex->mHeight;
                                    outTexture.pixels.resize(embTex->mWidth * embTex->mHeight * 4);
                                    for (u32 i = 0; i < embTex->mWidth * embTex->mHeight; ++i) {
                                        outTexture.pixels[i * 4 + 0] = embTex->pcData[i].r;
                                        outTexture.pixels[i * 4 + 1] = embTex->pcData[i].g;
                                        outTexture.pixels[i * 4 + 2] = embTex->pcData[i].b;
                                        outTexture.pixels[i * 4 + 3] = embTex->pcData[i].a;
                                    }
                                    return true;
                                }
                            }
                        }
                        
                        // Check candidate file paths
                        auto texFilename = std::filesystem::path(rawPath).filename();
                        std::vector<std::filesystem::path> candidates = {
                            meshDir / rawPath,
                            meshDir / texFilename,
                            meshDir / "Textures" / texFilename,
                            meshDir / "textures" / texFilename,
                            meshDir / "Texture" / texFilename,
                            meshDir / "Materials" / texFilename,
                            meshDir / "materials" / texFilename,
                            meshDir / "source" / texFilename,
                            meshDir.parent_path() / "Textures" / texFilename,
                            meshDir.parent_path() / "textures" / texFilename
                        };
                        
                        for (const auto& candidate : candidates) {
                            if (LoadTextureFromFile(candidate, outTexture)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 2. Generic recursive texture search across mesh directory and subdirectories
    std::string stem = meshPath.stem().string();
    std::string stemLower = stem;
    std::transform(stemLower.begin(), stemLower.end(), stemLower.begin(), ::tolower);
    
    // Extract primary keyword (e.g. "zombie", "barrel", "box", "rika", "y_bot")
    std::string firstWord = stemLower;
    size_t sepPos = firstWord.find_first_of("_ -@");
    if (sepPos != std::string::npos && sepPos > 1) {
        firstWord = firstWord.substr(0, sepPos);
    }
    
    struct TextureCandidate {
        std::filesystem::path path;
        int score = 0;
    };
    
    std::vector<TextureCandidate> candidates;
    
    std::error_code ec;
    if (std::filesystem::exists(meshDir, ec)) {
        for (std::filesystem::recursive_directory_iterator it(meshDir, ec), end; it != end; it.increment(ec)) {
            if (ec) break;
            if (it.depth() > 3) {
                it.pop();
                continue;
            }
            if (!it->is_regular_file(ec)) continue;
            
            std::string ext = it->path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" &&
                ext != ".tga" && ext != ".bmp" && ext != ".webp") {
                continue;
            }
            
            std::string fullPathLower = it->path().string();
            std::transform(fullPathLower.begin(), fullPathLower.end(), fullPathLower.begin(), ::tolower);
            
            std::string filenameLower = it->path().filename().string();
            std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
            
            // Skip thumbnails
            if (fullPathLower.find("thumb") != std::string::npos) continue;
            
            // Skip non-color maps
            if (filenameLower.find("norm") != std::string::npos ||
                filenameLower.find("nrm") != std::string::npos ||
                filenameLower.find("_n.") != std::string::npos ||
                filenameLower.find("rough") != std::string::npos ||
                filenameLower.find("_r.") != std::string::npos ||
                filenameLower.find("metal") != std::string::npos ||
                filenameLower.find("met.") != std::string::npos ||
                filenameLower.find("spec") != std::string::npos ||
                filenameLower.find("height") != std::string::npos ||
                filenameLower.find("disp") != std::string::npos ||
                filenameLower.find("ao.") != std::string::npos ||
                filenameLower.find("occ") != std::string::npos ||
                filenameLower.find("mask") != std::string::npos) {
                continue;
            }
            
            int score = 10;
            
            // Matches entire stem
            if (filenameLower.find(stemLower) != std::string::npos ||
                fullPathLower.find(stemLower) != std::string::npos) {
                score += 60;
            }
            // Matches first word keyword
            else if (!firstWord.empty() && (filenameLower.find(firstWord) != std::string::npos ||
                                           fullPathLower.find(firstWord) != std::string::npos)) {
                score += 40;
            }
            
            // Bonus for color / diffuse / albedo keywords
            if (filenameLower.find("diff") != std::string::npos ||
                filenameLower.find("albedo") != std::string::npos ||
                filenameLower.find("basecolor") != std::string::npos ||
                filenameLower.find("base_color") != std::string::npos ||
                filenameLower.find("_col") != std::string::npos ||
                filenameLower.find("color") != std::string::npos ||
                filenameLower.find("_d.") != std::string::npos ||
                filenameLower.find("_d_") != std::string::npos ||
                filenameLower.find("texture") != std::string::npos) {
                score += 30;
            }
            
            // Prefer textures closer in directory hierarchy
            score -= it.depth() * 5;
            
            candidates.push_back({it->path(), score});
        }
    }
    
    if (!candidates.empty()) {
        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
            return a.score > b.score;
        });
        
        for (const auto& cand : candidates) {
            if (LoadTextureFromFile(cand.path, outTexture)) {
                LUMA_LOG_INFO("MeshThumbnailRenderer", "Matched texture {} (score {}) for mesh {}",
                              cand.path.string(), cand.score, meshPath.string());
                return true;
            }
        }
    }
    
    return false;
}

bool MeshThumbnailRenderer::LoadMesh(const std::filesystem::path& path, LumaMeshData& outMesh, TexturePreviewData& outTexture) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".lmesh") {
        auto result = LumaMeshIO::ReadMesh(path);
        if (result) {
            outMesh = std::move(*result);
            FindAndLoadTexture(path, nullptr, outTexture);
            return true;
        }
        return false;
    }
    
    // Check if companion .lmesh exists in the same folder
    auto companion = path.parent_path() / (path.stem().string() + ".lmesh");
    if (std::filesystem::exists(companion)) {
        auto result = LumaMeshIO::ReadMesh(companion);
        if (result) {
            outMesh = std::move(*result);
            FindAndLoadTexture(path, nullptr, outTexture);
            return true;
        }
    }
    
    // Fallback: load directly using Assimp
    try {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            path.string(),
            aiProcess_Triangulate | aiProcess_GenSmoothNormals |
            aiProcess_JoinIdenticalVertices | aiProcess_OptimizeMeshes
        );
        
        if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0) {
            LUMA_LOG_ERROR("MeshThumbnailRenderer", "Assimp failed to load {}: {}",
                           path.string(), importer.GetErrorString());
            return false;
        }
        
        outMesh.name = path.stem().string();
        outMesh.vertices.clear();
        outMesh.indices.clear();
        
        Math::Vec3 minPos{1e9f, 1e9f, 1e9f};
        Math::Vec3 maxPos{-1e9f, -1e9f, -1e9f};
        
        for (u32 m = 0; m < scene->mNumMeshes; ++m) {
            const aiMesh* mesh = scene->mMeshes[m];
            u32 baseVertex = static_cast<u32>(outMesh.vertices.size());
            
            for (u32 v = 0; v < mesh->mNumVertices; ++v) {
                LumaVertex vertex{};
                vertex.position = Math::Vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
                if (mesh->HasNormals()) {
                    vertex.normal = Math::Vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
                }
                if (mesh->HasTextureCoords(0)) {
                    vertex.texCoord = Math::Vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
                }
                outMesh.vertices.push_back(vertex);
                
                minPos.x = std::min(minPos.x, vertex.position.x);
                minPos.y = std::min(minPos.y, vertex.position.y);
                minPos.z = std::min(minPos.z, vertex.position.z);
                maxPos.x = std::max(maxPos.x, vertex.position.x);
                maxPos.y = std::max(maxPos.y, vertex.position.y);
                maxPos.z = std::max(maxPos.z, vertex.position.z);
            }
            
            for (u32 f = 0; f < mesh->mNumFaces; ++f) {
                const aiFace& face = mesh->mFaces[f];
                if (face.mNumIndices == 3) {
                    outMesh.indices.push_back(baseVertex + face.mIndices[0]);
                    outMesh.indices.push_back(baseVertex + face.mIndices[1]);
                    outMesh.indices.push_back(baseVertex + face.mIndices[2]);
                }
            }
        }
        
        if (outMesh.vertices.empty() || outMesh.indices.empty()) {
            return false;
        }
        
        outMesh.bounds.min = minPos;
        outMesh.bounds.max = maxPos;
        outMesh.bounds.center = (minPos + maxPos) * 0.5f;
        Math::Vec3 diff = maxPos - minPos;
        outMesh.bounds.radius = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z) * 0.5f;
        if (outMesh.bounds.radius < 0.001f) outMesh.bounds.radius = 1.0f;
        
        // Create a single submesh for the entire loaded mesh (required by LumaMeshData::IsValid())
        LumaSubmesh submesh;
        submesh.indexOffset = 0;
        submesh.indexCount = static_cast<u32>(outMesh.indices.size());
        submesh.vertexOffset = 0;
        submesh.vertexCount = static_cast<u32>(outMesh.vertices.size());
        submesh.materialName = "Default";
        submesh.bounds = outMesh.bounds;
        outMesh.submeshes.push_back(submesh);
        
        // Find and load texture using Assimp material and directory search
        FindAndLoadTexture(path, scene, outTexture);
        
        return true;
    } catch (const std::exception& e) {
        LUMA_LOG_ERROR("MeshThumbnailRenderer", "Exception loading mesh {}: {}", path.string(), e.what());
        return false;
    }
}

bool MeshThumbnailRenderer::RenderWireframe(const LumaMeshData& mesh,
                                            std::vector<u8>& outPixels,
                                            u32 width,
                                            u32 height) {
    outPixels.resize(width * height * 4);
    
    // Clear to dark sleek background
    ClearBackground(outPixels, width, height, 26, 28, 34);
    
    // Simple orthographic projection
    Math::Mat4 viewMatrix = Math::Mat4::Identity();
    Math::Mat4 projMatrix = Math::Mat4::Identity();
    
    // Center camera on mesh bounds
    Math::Vec3 center = mesh.bounds.center;
    f32 radius = mesh.bounds.radius > 0.001f ? mesh.bounds.radius : 1.0f;
    
    // Set up view matrix (look at mesh from diagonal)
    Math::Vec3 eye = center + Math::Vec3(radius * 1.5f, radius * 1.5f, radius * 1.5f);
    Math::Vec3 up = Math::Vec3(0.0f, 0.0f, 1.0f);
    
    // Simple look-at matrix (column-major)
    Math::Vec3 f = Math::Normalize(center - eye);
    Math::Vec3 s = Math::Normalize(Math::Cross(f, up));
    Math::Vec3 u = Math::Cross(s, f);
    
    viewMatrix = Math::Mat4::Identity();
    viewMatrix.m[0] = s.x; viewMatrix.m[4] = s.y; viewMatrix.m[8] = s.z;
    viewMatrix.m[1] = u.x; viewMatrix.m[5] = u.y; viewMatrix.m[9] = u.z;
    viewMatrix.m[2] = -f.x; viewMatrix.m[6] = -f.y; viewMatrix.m[10] = -f.z;
    viewMatrix.m[12] = -Math::Dot(s, eye);
    viewMatrix.m[13] = -Math::Dot(u, eye);
    viewMatrix.m[14] = Math::Dot(f, eye);
    
    // Orthographic projection (column-major)
    f32 orthoSize = radius * 2.3f;
    f32 aspect = static_cast<f32>(width) / static_cast<f32>(height);
    
    projMatrix = Math::Mat4::Identity();
    projMatrix.m[0] = 2.0f / (orthoSize * aspect);
    projMatrix.m[5] = 2.0f / orthoSize;
    projMatrix.m[10] = -2.0f / (orthoSize * 2.0f);
    projMatrix.m[15] = 1.0f;
    
    // Transform vertices
    std::vector<Math::Vec3> transformedVerts;
    TransformMesh(mesh, transformedVerts, viewMatrix, projMatrix);
    
    // Scale to screen space
    for (auto& v : transformedVerts) {
        v.x = (v.x + 1.0f) * 0.5f * width;
        v.y = (1.0f - v.y) * 0.5f * height;  // Flip Y for screen space
    }
    
    // Draw wireframe edges
    for (u32 i = 0; i < mesh.indices.size(); i += 3) {
        u32 i0 = mesh.indices[i];
        u32 i1 = mesh.indices[i + 1];
        u32 i2 = mesh.indices[i + 2];
        
        if (i0 >= transformedVerts.size() || i1 >= transformedVerts.size() || i2 >= transformedVerts.size()) {
            continue;
        }
        
        Math::Vec3 v0 = transformedVerts[i0];
        Math::Vec3 v1 = transformedVerts[i1];
        Math::Vec3 v2 = transformedVerts[i2];
        
        // Draw edges with modern light blue / cyan wireframe color
        DrawLine(outPixels, width, height, v0.x, v0.y, v1.x, v1.y, 130, 195, 255);
        DrawLine(outPixels, width, height, v1.x, v1.y, v2.x, v2.y, 130, 195, 255);
        DrawLine(outPixels, width, height, v2.x, v2.y, v0.x, v0.y, 130, 195, 255);
    }
    
    return true;
}

bool MeshThumbnailRenderer::RenderSolid(const LumaMeshData& mesh,
                                        const TexturePreviewData* texture,
                                        std::vector<u8>& outPixels,
                                        u32 width,
                                        u32 height) {
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        return false;
    }

    outPixels.resize(width * height * 4);
    std::vector<f32> depthBuffer(width * height, 1e9f);
    
    // Gradient studio background
    for (u32 y = 0; y < height; ++y) {
        f32 t = static_cast<f32>(y) / static_cast<f32>(height);
        u8 r = static_cast<u8>(36.0f * (1.0f - t) + 20.0f * t);
        u8 g = static_cast<u8>(40.0f * (1.0f - t) + 22.0f * t);
        u8 b = static_cast<u8>(48.0f * (1.0f - t) + 28.0f * t);
        for (u32 x = 0; x < width; ++x) {
            u32 idx = (y * width + x) * 4;
            outPixels[idx + 0] = r;
            outPixels[idx + 1] = g;
            outPixels[idx + 2] = b;
            outPixels[idx + 3] = 255;
        }
    }
    
    // Set up camera / view / projection
    Math::Vec3 center = mesh.bounds.center;
    f32 radius = mesh.bounds.radius > 0.001f ? mesh.bounds.radius : 1.0f;
    
    // Diagonal camera angle (elevated 3/4 view)
    Math::Vec3 eye = center + Math::Vec3(radius * 1.4f, radius * 1.4f, radius * 1.2f);
    Math::Vec3 up = Math::Vec3(0.0f, 0.0f, 1.0f);
    
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
    
    f32 orthoSize = radius * 2.2f;
    f32 aspect = static_cast<f32>(width) / static_cast<f32>(height);
    
    Math::Mat4 projMatrix = Math::Mat4::Identity();
    projMatrix.m[0] = 2.0f / (orthoSize * aspect);
    projMatrix.m[5] = 2.0f / orthoSize;
    projMatrix.m[10] = -1.0f / (radius * 4.0f);
    projMatrix.m[15] = 1.0f;
    
    Math::Mat4 mvp = projMatrix * viewMatrix;
    
    // Transform vertices to screen space + depth + UVs
    struct ScreenVertex {
        f32 x, y, z;
        Math::Vec3 normal;
        Math::Vec2 uv;
    };
    
    std::vector<ScreenVertex> screenVerts(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
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
    
    // Key light direction & fill light
    Math::Vec3 lightDir1 = Math::Normalize(Math::Vec3(0.5f, 0.8f, 1.0f));
    Math::Vec3 lightDir2 = Math::Normalize(Math::Vec3(-0.7f, -0.4f, 0.3f));
    
    // Texture sampling lambda
    auto sampleTexture = [&](f32 u, f32 v) -> Math::Vec3 {
        if (!texture || !texture->IsValid()) {
            return Math::Vec3{0.78f, 0.80f, 0.84f};
        }
        
        // Wrap UV
        u = u - std::floor(u);
        v = v - std::floor(v);
        
        f32 tx = u * static_cast<f32>(texture->width - 1);
        f32 ty = (1.0f - v) * static_cast<f32>(texture->height - 1);
        
        u32 px = std::clamp(static_cast<u32>(tx + 0.5f), 0u, texture->width - 1);
        u32 py = std::clamp(static_cast<u32>(ty + 0.5f), 0u, texture->height - 1);
        
        u32 idx = (py * texture->width + px) * 4;
        f32 r = static_cast<f32>(texture->pixels[idx + 0]) / 255.0f;
        f32 g = static_cast<f32>(texture->pixels[idx + 1]) / 255.0f;
        f32 b = static_cast<f32>(texture->pixels[idx + 2]) / 255.0f;
        
        return Math::Vec3{r, g, b};
    };
    
    // Rasterize triangles using barycentric coordinates
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        u32 i0 = mesh.indices[i];
        u32 i1 = mesh.indices[i + 1];
        u32 i2 = mesh.indices[i + 2];
        if (i0 >= screenVerts.size() || i1 >= screenVerts.size() || i2 >= screenVerts.size()) continue;
        
        const auto& v0 = screenVerts[i0];
        const auto& v1 = screenVerts[i1];
        const auto& v2 = screenVerts[i2];
        
        // Signed area
        f32 area = (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x);
        if (std::abs(area) < 0.0001f) continue;
        f32 invArea = 1.0f / area;
        
        // Fallback face normal in world space
        Math::Vec3 p0 = mesh.vertices[i0].position;
        Math::Vec3 p1 = mesh.vertices[i1].position;
        Math::Vec3 p2 = mesh.vertices[i2].position;
        Math::Vec3 fn = Math::Normalize(Math::Cross(p1 - p0, p2 - p0));
        
        // Bounding box in screen space
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
                        
                        // Interpolate normal
                        Math::Vec3 norm;
                        if (v0.normal.x != 0.0f || v0.normal.y != 0.0f || v0.normal.z != 0.0f) {
                            norm = Math::Normalize(v0.normal * w0 + v1.normal * w1 + v2.normal * w2);
                        } else {
                            norm = fn;
                        }
                        
                        // Sample texture or base color
                        Math::Vec2 uv = v0.uv * w0 + v1.uv * w1 + v2.uv * w2;
                        Math::Vec3 baseColor = sampleTexture(uv.x, uv.y);
                        
                        // Studio lighting
                        f32 diff1 = std::max(0.0f, Math::Dot(norm, lightDir1));
                        f32 diff2 = std::max(0.0f, Math::Dot(norm, lightDir2)) * 0.30f;
                        f32 ambient = 0.35f;
                        
                        // Subtle rim light
                        Math::Vec3 viewDir = Math::Normalize(eye - center);
                        f32 rim = std::pow(1.0f - std::max(0.0f, Math::Dot(norm, viewDir)), 3.0f) * 0.20f;
                        
                        f32 intensity = std::clamp(diff1 * 0.70f + diff2 + ambient + rim, 0.0f, 1.25f);
                        
                        outPixels[pidx * 4 + 0] = static_cast<u8>(std::clamp(baseColor.x * intensity * 255.0f, 0.0f, 255.0f));
                        outPixels[pidx * 4 + 1] = static_cast<u8>(std::clamp(baseColor.y * intensity * 255.0f, 0.0f, 255.0f));
                        outPixels[pidx * 4 + 2] = static_cast<u8>(std::clamp(baseColor.z * intensity * 255.0f, 0.0f, 255.0f));
                        outPixels[pidx * 4 + 3] = 255;
                    }
                }
            }
        }
    }
    
    return true;
}

void MeshThumbnailRenderer::TransformMesh(const LumaMeshData& mesh,
                                          std::vector<Math::Vec3>& outVertices,
                                          const Math::Mat4& viewMatrix,
                                          const Math::Mat4& projMatrix) {
    Math::Mat4 mvp = projMatrix * viewMatrix;
    
    outVertices.resize(mesh.vertices.size());
    
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        
        // Manual matrix-vector multiplication
        f32 x = v.position.x;
        f32 y = v.position.y;
        f32 z = v.position.z;
        f32 w = 1.0f;
        
        f32 tx = mvp.m[0] * x + mvp.m[4] * y + mvp.m[8] * z + mvp.m[12] * w;
        f32 ty = mvp.m[1] * x + mvp.m[5] * y + mvp.m[9] * z + mvp.m[13] * w;
        f32 tz = mvp.m[2] * x + mvp.m[6] * y + mvp.m[10] * z + mvp.m[14] * w;
        f32 tw = mvp.m[3] * x + mvp.m[7] * y + mvp.m[11] * z + mvp.m[15] * w;
        
        // Perspective divide
        if (tw != 0.0f) {
            tx /= tw;
            ty /= tw;
            tz /= tw;
        }
        
        outVertices[i] = Math::Vec3(tx, ty, tz);
    }
}

void MeshThumbnailRenderer::ClearBackground(std::vector<u8>& pixels, u32 width, u32 height, u8 r, u8 g, u8 b) {
    for (u32 i = 0; i < width * height; ++i) {
        pixels[i * 4 + 0] = r;
        pixels[i * 4 + 1] = g;
        pixels[i * 4 + 2] = b;
        pixels[i * 4 + 3] = 255;
    }
}

void MeshThumbnailRenderer::DrawLine(std::vector<u8>& pixels, u32 width, u32 height,
                                     f32 x0, f32 y0, f32 x1, f32 y1,
                                     u8 r, u8 g, u8 b) {
    // Bresenham's line algorithm
    i32 ix0 = static_cast<i32>(std::round(x0));
    i32 iy0 = static_cast<i32>(std::round(y0));
    i32 ix1 = static_cast<i32>(std::round(x1));
    i32 iy1 = static_cast<i32>(std::round(y1));
    
    i32 dx = std::abs(ix1 - ix0);
    i32 dy = std::abs(iy1 - iy0);
    i32 sx = ix0 < ix1 ? 1 : -1;
    i32 sy = iy0 < iy1 ? 1 : -1;
    i32 err = dx - dy;
    
    while (true) {
        if (ix0 >= 0 && ix0 < static_cast<i32>(width) && iy0 >= 0 && iy0 < static_cast<i32>(height)) {
            u32 offset = (iy0 * width + ix0) * 4;
            pixels[offset + 0] = r;
            pixels[offset + 1] = g;
            pixels[offset + 2] = b;
            pixels[offset + 3] = 255;
        }
        
        if (ix0 == ix1 && iy0 == iy1) break;
        
        i32 e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            ix0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            iy0 += sy;
        }
    }
}

} // namespace Luma