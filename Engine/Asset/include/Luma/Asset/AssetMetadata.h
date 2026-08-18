#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "Luma/Asset/AssetId.h"
#include "Luma/Asset/AssetType.h"
#include "Luma/Core/Types.h"

// Asset metadata stored alongside source assets in .meta files.
// Tracks import settings, GUIDs, source/derived relationships, and
// import state. Used by the import pipeline to determine when reimport
// is needed and to persist user-configured import settings.

namespace Luma {

struct AssetMetadata {
    // Stable GUID for this asset (independent of filename/renames)
    AssetId guid{};
    
    // Asset type (Mesh, Texture, Material, etc.)
    AssetType type = AssetType::Unknown;
    
    // Original source file path (absolute, normalized)
    std::filesystem::path sourcePath;
    
    // Derived/native asset path (e.g., .lmesh for meshes)
    std::filesystem::path nativePath;
    
    // Importer version (for detecting when importer changes)
    std::string importerVersion;
    
    // Source file modification time at last successful import
    i64 sourceMtime = 0;
    
    // Source file hash (for detecting content changes)
    std::string sourceHash;
    
    // Import timestamp
    i64 importTime = 0;
    
    // Whether this asset is currently being imported
    bool isImporting = false;
    
    // Last import error message (empty if successful)
    std::string lastError;
    
    // Import settings as JSON string (type-specific settings)
    std::string importSettings;
    
    // Whether this metadata file is valid/outdated
    bool IsValid() const noexcept { return guid.IsValid(); }
    
    // Check if source has changed since last import
    bool SourceChanged(i64 currentMtime, const std::string& currentHash) const;
};

// Mesh-specific import settings
struct MeshImportSettings {
    bool generateNormals = true;
    bool generateTangents = true;
    bool flipUVs = false;
    bool optimizeMesh = true;
    bool optimizeVertexCache = true;
    bool optimizeVertexFetch = true;
    float scale = 1.0f;
    bool importSkeleton = true;
    bool importAnimations = true;
    
    // Convert to/from JSON for storage in metadata
    std::string ToJson() const;
    static MeshImportSettings FromJson(const std::string& json);
    static MeshImportSettings GetDefaults();
};

// Texture-specific import settings
struct TextureImportSettings {
    bool generateMipmaps = true;
    bool sRGB = true;
    bool compress = true;
    bool normalMap = false;
    i32 maxTextureSize = 4096;
    i32 desiredSize = 0;
    f32 scale = 1.0f;
    i32 filterType = 0;
    bool preserveAlpha = true;
    bool flipY = false;
    bool premultiplyAlpha = false;
    i32 alphaThreshold = 0;
    bool packChannels = false;
    i32 redChannel = 0;
    i32 greenChannel = 1;
    i32 blueChannel = 2;
    i32 alphaChannel = 3;
    
    // Convert to/from JSON for storage in metadata
    std::string ToJson() const;
    static TextureImportSettings FromJson(const std::string& json);
    static TextureImportSettings GetDefaults();
};

// Metadata file operations
namespace AssetMetadataIO {
    // Read metadata from a .meta file. Returns nullopt if file doesn't exist or is invalid.
    std::optional<AssetMetadata> Read(const std::filesystem::path& metaPath);
    
    // Write metadata to a .meta file. Returns false on failure.
    bool Write(const std::filesystem::path& metaPath, const AssetMetadata& meta);
    
    // Get the expected .meta file path for a source asset
    std::filesystem::path MetaPathForSource(const std::filesystem::path& sourcePath);
    
    // Get the expected native asset path for a source asset (e.g., .fbx -> .lmesh)
    std::filesystem::path NativePathForSource(const std::filesystem::path& sourcePath, AssetType type);
    
    // Compute a simple hash of a file's contents
    std::string ComputeFileHash(const std::filesystem::path& filePath);
}

}  // namespace Luma
