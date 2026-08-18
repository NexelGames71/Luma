#pragma once

#include <memory>
#include <string>
#include <filesystem>

#include "Luma/Asset/AssetImportManager.h"
#include "Luma/Asset/AssetMetadata.h"
#include "Luma/Asset/LumaMesh.h"
#include "Luma/Asset/Factory.h"

// Forward declarations for Assimp types
struct aiScene;
struct aiMesh;
struct aiNode;
struct aiMaterial;
struct aiString;

// Mesh Importer - converts Assimp-loaded meshes to Luma native .lmesh format
// Implements both IAssetImporter (legacy) and AssetFactory (new pattern) interfaces

namespace Luma {

class MeshImporter : public IAssetImporter, public ReimportFactory {
public:
    MeshImporter();
    ~MeshImporter() override;
    
    // IAssetImporter interface (legacy compatibility)
    AssetType GetAssetType() const override { return AssetType::Mesh; }
    std::string GetImporterVersion() const override { return "1.0.0"; }
    bool CanImport(const std::filesystem::path& sourcePath) const override;
    ImportResult Import(const std::filesystem::path& sourcePath,
                       const std::string& importSettings,
                       const std::filesystem::path& outputDir) override;
    std::string GetDefaultSettings() const override;
    
    // AssetFactory interface (new enhanced pattern)
    std::vector<std::string> GetSupportedExtensions() const override;
    i32 GetPriority() const override { return 100; } // High priority for mesh formats
    std::string GetFactoryName() const override { return "MeshImporter"; }
    std::string GetFactoryVersion() const override { return "1.0.0"; }
    std::string GetDescription() const override { return "Imports 3D meshes from FBX, OBJ, GLTF, and other formats"; }
    bool ValidateSource(const std::filesystem::path& sourcePath) const override;
    
    // ReimportFactory interface
    bool NeedsReimport(const AssetId& assetId) const override;
    std::string GetCurrentSettings(const AssetId& assetId) const override;
    ImportResult Reimport(const AssetId& assetId, const std::string& importSettings) override;
    void CleanupDerivedFiles(const AssetId& assetId) override;
    std::filesystem::path GetSourcePath(const AssetId& assetId) const override;
    
private:
    // Load mesh using Assimp and convert to LumaMeshData
    std::optional<LumaMeshData> LoadWithAssimp(const std::filesystem::path& sourcePath,
                                               const MeshImportSettings& settings);
    
    // Convert Assimp scene to Luma mesh data
    bool ConvertAssimpToLuma(const struct aiScene* aiScene,
                            const std::string& name,
                            const MeshImportSettings& settings,
                            LumaMeshData& outMesh);
    
    // Process a single Assimp mesh
    bool ProcessAssimpMesh(const struct aiMesh* mesh,
                          const struct aiScene* scene,
                          const MeshImportSettings& settings,
                          LumaMeshData& outMesh,
                          u32& vertexOffset,
                          u32& indexOffset);
    
    // Process node hierarchy
    void ProcessAssimpNodes(const struct aiNode* node,
                          i32 parentIndex,
                          std::vector<LumaNode>& outNodes);
    
    // Apply import settings to mesh data
    void ApplyImportSettings(LumaMeshData& mesh, const MeshImportSettings& settings);
    
    // Generate normals if missing
    void GenerateNormals(LumaMeshData& mesh);
    
    // Generate tangents if missing
    void GenerateTangents(LumaMeshData& mesh);
    
    // Flip UV coordinates
    void FlipUVs(LumaMeshData& mesh);
    
    // Apply scale transform
    void ApplyScale(LumaMeshData& mesh, f32 scale);
    
    // Optimize mesh (vertex cache, etc.)
    void OptimizeMesh(LumaMeshData& mesh);
    
    // Update metadata after successful import
    bool UpdateMetadata(const std::filesystem::path& sourcePath,
                      const std::filesystem::path& nativePath,
                      const MeshImportSettings& settings,
                      const std::string& importerVersion);
    
    // Asset registry pointer for reimport operations
    AssetRegistry* m_registry = nullptr;
    
    // Set the asset registry (called by import manager)
    void SetRegistry(AssetRegistry* registry) { m_registry = registry; }
};

}  // namespace Luma
