#include "Luma/Asset/MeshImporter.h"
#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Asset/AssetMetadata.h"
#include "Luma/Asset/LumaMesh.h"
#include "Luma/Asset/LumaMeshFormat.h"

#include <filesystem>
#include <chrono>
#include <algorithm>

#include "Luma/Core/Log.h"

// Assimp for mesh loading
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>

namespace Luma {

// ============================================================================
// MeshImporter Implementation
// ============================================================================

MeshImporter::MeshImporter() {
    LUMA_LOG_INFO("MeshImporter", "Initialized");
}

MeshImporter::~MeshImporter() {
    LUMA_LOG_INFO("MeshImporter", "Shutdown");
}

std::vector<std::string> MeshImporter::GetSupportedExtensions() const {
    return {".fbx", ".obj", ".gltf", ".glb", ".dae", ".blend", ".3ds", ".ase", ".ifc"};
}

bool MeshImporter::CanImport(const std::filesystem::path& sourcePath) const {
    if (!std::filesystem::exists(sourcePath)) {
        return false;
    }
    return HasSupportedExtension(sourcePath);
}

ImportResult MeshImporter::Import(const std::filesystem::path& sourcePath,
                                   const std::string& importSettings,
                                   const std::filesystem::path& outputDir) {
    ImportResult result;
    result.status = ImportStatus::Failed;
    result.sourcePath = sourcePath;
    
    LUMA_LOG_INFO("MeshImporter", "Importing mesh: {}", sourcePath.string());
    
    // Parse import settings
    MeshImportSettings settings = MeshImportSettings::GetDefaults();
    if (!importSettings.empty()) {
        settings = MeshImportSettings::FromJson(importSettings);
    }
    
    // Load mesh using Assimp
    Assimp::Importer importer;
    
    unsigned int flags = aiProcess_Triangulate | aiProcess_FlipUVs;
    
    if (settings.generateNormals) {
        flags |= aiProcess_GenNormals;
    }
    if (settings.generateTangents) {
        flags |= aiProcess_CalcTangentSpace;
    }
    if (settings.optimizeMesh) {
        flags |= aiProcess_ImproveCacheLocality;
    }
    if (settings.optimizeVertexCache) {
        flags |= aiProcess_OptimizeGraph;
    }
    if (settings.optimizeVertexFetch) {
        flags |= aiProcess_OptimizeMeshes;
    }
    
    const aiScene* scene = importer.ReadFile(sourcePath.string().c_str(), flags);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        result.errorMessage = "Failed to load mesh: " + std::string(importer.GetErrorString());
        return result;
    }
    
    if (!scene->HasMeshes()) {
        result.errorMessage = "Mesh file contains no meshes";
        return result;
    }
    
    // Convert to LumaMeshData
    LumaMeshData meshData;
    meshData.name = sourcePath.stem().string();
    meshData.sourceFile = sourcePath.string();
    meshData.importerVersion = GetImporterVersion();
    
    u32 totalVertices = 0;
    u32 totalIndices = 0;
    
    // Process all meshes
    for (u32 meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx) {
        const aiMesh* mesh = scene->mMeshes[meshIdx];
        
        LumaSubmesh submesh;
        submesh.vertexOffset = totalVertices;
        submesh.vertexCount = mesh->mNumVertices;
        submesh.indexOffset = totalIndices;
        // Material reference from the source file (assimp resolves the .mtl
        // `newmtl` name for OBJ, the material name for FBX/glTF, ...). Stored
        // on the submesh so the editor can match MTL materials to geometry.
        submesh.materialName = "";
        if (mesh->mMaterialIndex < scene->mNumMaterials &&
            scene->mMaterials[mesh->mMaterialIndex]) {
            aiString name = scene->mMaterials[mesh->mMaterialIndex]->GetName();
            submesh.materialName = name.C_Str();
        }
        
        // Add vertices
        for (u32 v = 0; v < mesh->mNumVertices; ++v) {
            LumaVertex vertex;
            
            // Position
            vertex.position.x = mesh->mVertices[v].x * settings.scale;
            vertex.position.y = mesh->mVertices[v].y * settings.scale;
            vertex.position.z = mesh->mVertices[v].z * settings.scale;
            
            // Normal
            if (mesh->HasNormals()) {
                vertex.normal.x = mesh->mNormals[v].x;
                vertex.normal.y = mesh->mNormals[v].y;
                vertex.normal.z = mesh->mNormals[v].z;
            } else {
                vertex.normal = Math::Vec3(0.0f, 1.0f, 0.0f);
            }
            
            // Tangent
            if (mesh->HasTangentsAndBitangents()) {
                vertex.tangent.x = mesh->mTangents[v].x;
                vertex.tangent.y = mesh->mTangents[v].y;
                vertex.tangent.z = mesh->mTangents[v].z;
            } else {
                vertex.tangent = Math::Vec3(1.0f, 0.0f, 0.0f);
            }
            
            // Bitangent (compute from normal and tangent)
            vertex.bitangent = Math::Cross(vertex.normal, vertex.tangent);
            
            // TexCoord
            if (mesh->HasTextureCoords(0)) {
                vertex.texCoord.x = mesh->mTextureCoords[0][v].x;
                vertex.texCoord.y = settings.flipUVs ? (1.0f - mesh->mTextureCoords[0][v].y) : mesh->mTextureCoords[0][v].y;
            } else {
                vertex.texCoord = Math::Vec2(0.0f, 0.0f);
            }
            
            // Color (default to white; read the first vertex-color set when
            // the source mesh carries one).
            vertex.color = Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
            if (mesh->HasVertexColors(0)) {
                vertex.color.x = mesh->mColors[0][v].r;
                vertex.color.y = mesh->mColors[0][v].g;
                vertex.color.z = mesh->mColors[0][v].b;
                vertex.color.w = mesh->mColors[0][v].a;
            }
            
            meshData.vertices.push_back(vertex);
        }
        
        // Add indices
        for (u32 f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            for (u32 i = 0; i < face.mNumIndices; ++i) {
                meshData.indices.push_back(face.mIndices[i] + totalVertices);
            }
        }
        
        submesh.indexCount = mesh->mNumFaces * 3;
        meshData.submeshes.push_back(submesh);
        
        totalVertices += mesh->mNumVertices;
        totalIndices += submesh.indexCount;
    }
    
    // Compute bounds
    meshData.ComputeBounds();
    
    LUMA_LOG_INFO("MeshImporter", "Loaded mesh: {} vertices, {} indices, {} submeshes",
                  totalVertices, totalIndices, meshData.submeshes.size());
    
    // Generate output path
    std::string outputFilename = sourcePath.stem().string() + ".lmesh";
    auto outputPath = outputDir / outputFilename;
    
    // Save mesh in Luma format
    if (!LumaMeshIO::WriteMesh(outputPath, meshData)) {
        result.errorMessage = "Failed to save mesh: " + outputPath.string();
        return result;
    }
    
    // Update metadata
    if (!UpdateMetadata(sourcePath, outputPath, settings, GetImporterVersion())) {
        result.errorMessage = "Failed to update metadata";
        return result;
    }
    
    result.status = ImportStatus::Completed;
    result.nativePath = outputPath;
    result.assetId = MakeAssetIdFromKey(sourcePath.string());
    
    LUMA_LOG_INFO("MeshImporter", "Successfully imported mesh: {}", sourcePath.string());
    return result;
}

bool MeshImporter::ValidateSource(const std::filesystem::path& sourcePath) const {
    if (!AssetFactory::ValidateSource(sourcePath)) {
        return false;
    }
    
    // Try to load with Assimp to validate it's a valid mesh file
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(sourcePath.string().c_str(), 
                                                aiProcess_Triangulate);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        return false;
    }
    
    return true;
}

// ============================================================================
// ReimportFactory Interface Implementation
// ============================================================================

bool MeshImporter::NeedsReimport(const AssetId& assetId) const {
    if (!m_registry) {
        return false;
    }
    
    auto meta = m_registry->GetMetadata(assetId);
    if (!meta) {
        return false;
    }
    
    if (!std::filesystem::exists(meta->sourcePath)) {
        return false;
    }
    
    std::error_code ec;
    auto currentMtime = std::chrono::duration_cast<std::chrono::seconds>(
        std::filesystem::last_write_time(meta->sourcePath, ec).time_since_epoch()).count();
    
    if (currentMtime != meta->sourceMtime) {
        return true;
    }
    
    auto currentHash = AssetMetadataIO::ComputeFileHash(meta->sourcePath);
    if (currentHash != meta->sourceHash) {
        return true;
    }
    
    if (GetImporterVersion() != meta->importerVersion) {
        return true;
    }
    
    return false;
}

std::string MeshImporter::GetCurrentSettings(const AssetId& assetId) const {
    if (!m_registry) {
        return GetDefaultSettings();
    }
    
    auto meta = m_registry->GetMetadata(assetId);
    if (!meta) {
        return GetDefaultSettings();
    }
    
    return meta->importSettings;
}

ImportResult MeshImporter::Reimport(const AssetId& assetId, const std::string& importSettings) {
    ImportResult result;
    result.status = ImportStatus::Failed;
    result.assetId = assetId;
    
    if (!m_registry) {
        result.errorMessage = "Asset registry not set";
        return result;
    }
    
    auto meta = m_registry->GetMetadata(assetId);
    if (!meta) {
        result.errorMessage = "Asset metadata not found";
        return result;
    }
    
    if (!std::filesystem::exists(meta->sourcePath)) {
        result.errorMessage = "Source file not found: " + meta->sourcePath.string();
        return result;
    }
    
    std::string settingsToUse = importSettings.empty() ? meta->importSettings : importSettings;
    auto outputDir = meta->nativePath.parent_path();
    
    auto importResult = Import(meta->sourcePath, settingsToUse, outputDir);
    importResult.assetId = assetId;
    
    return importResult;
}

void MeshImporter::CleanupDerivedFiles(const AssetId& assetId) {
    if (!m_registry) {
        return;
    }
    
    auto meta = m_registry->GetMetadata(assetId);
    if (!meta) {
        return;
    }
    
    std::error_code ec;
    
    if (std::filesystem::exists(meta->nativePath, ec)) {
        std::filesystem::remove(meta->nativePath, ec);
    }
    
    auto metaPath = AssetMetadataIO::MetaPathForSource(meta->sourcePath);
    if (std::filesystem::exists(metaPath, ec)) {
        std::filesystem::remove(metaPath, ec);
    }
}

// ============================================================================
// Private Helper Functions
// ============================================================================

std::string MeshImporter::GetDefaultSettings() const {
    return MeshImportSettings::GetDefaults().ToJson();
}

bool MeshImporter::UpdateMetadata(const std::filesystem::path& sourcePath,
                                 const std::filesystem::path& nativePath,
                                 const MeshImportSettings& settings,
                                 const std::string& importerVersion) {
    AssetMetadata meta;
    meta.guid = MakeAssetIdFromKey(sourcePath.string());
    meta.type = AssetType::Mesh;
    meta.sourcePath = sourcePath;
    meta.nativePath = nativePath;
    meta.importerVersion = importerVersion;
    meta.importSettings = settings.ToJson();
    meta.isImporting = false;
    meta.lastError.clear();
    
    // Get current file metadata
    std::error_code ec;
    meta.sourceMtime = std::chrono::duration_cast<std::chrono::seconds>(
        std::filesystem::last_write_time(sourcePath, ec).time_since_epoch()).count();
    meta.sourceHash = AssetMetadataIO::ComputeFileHash(sourcePath);
    meta.importTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Write metadata file
    auto metaPath = AssetMetadataIO::MetaPathForSource(sourcePath);
    if (!AssetMetadataIO::Write(metaPath, meta)) {
        LUMA_LOG_ERROR("MeshImporter", "Failed to write metadata file: {}", metaPath.string());
        return false;
    }
    
    // Update registry if available
    if (m_registry) {
        m_registry->SaveMetadata(meta.guid, meta);
    }
    
    return true;
}

std::filesystem::path MeshImporter::GetSourcePath(const AssetId& assetId) const {
    if (!m_registry) return {};
    auto meta = m_registry->GetMetadata(assetId);
    if (meta && !meta->sourcePath.empty()) {
        return meta->sourcePath;
    }
    return {};
}

} // namespace Luma