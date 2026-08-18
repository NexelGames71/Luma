#pragma once

#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <fstream>

#include "Luma/Asset/AssetType.h"
#include "Luma/Asset/AssetId.h"
#include "Luma/Core/Types.h"

namespace Luma {

// Forward declarations
class AssetRegistry;
struct ImportResult;

/**
 * Base factory class for asset creation and import
 * Inspired by UE5's UFactory pattern
 */
class AssetFactory {
public:
    virtual ~AssetFactory() = default;
    
    /**
     * Get the supported file extensions for this factory
     * @return Vector of supported extensions (e.g., {".fbx", ".obj"})
     */
    virtual std::vector<std::string> GetSupportedExtensions() const = 0;
    
    /**
     * Get the asset type this factory creates
     */
    virtual AssetType GetAssetType() const = 0;
    
    /**
     * Get the factory priority (higher priority factories are tried first)
     */
    virtual i32 GetPriority() const { return 0; }
    
    /**
     * Check if this factory can handle the given file
     * @param sourcePath Path to the source file
     * @return true if this factory can import the file
     */
    virtual bool CanImport(const std::filesystem::path& sourcePath) const;
    
    /**
     * Import the asset (main import function)
     * @param sourcePath Path to the source file
     * @param importSettings JSON string with import settings
     * @param outputDir Directory where output files should be created
     * @return Import result with status and error information
     */
    virtual ImportResult Import(const std::filesystem::path& sourcePath,
                                const std::string& importSettings,
                                const std::filesystem::path& outputDir) = 0;
    
    /**
     * Reimport an existing asset
     * @param assetId ID of the asset to reimport
     * @param importSettings JSON string with import settings
     * @return Import result with status and error information
     */
    virtual ImportResult Reimport(const AssetId& assetId,
                                 const std::string& importSettings);
    
    /**
     * Get default import settings for this factory
     * @return JSON string with default settings
     */
    virtual std::string GetDefaultSettings() const = 0;
    
    /**
     * Get factory version for detecting when importer logic changes
     */
    virtual std::string GetFactoryVersion() const { return "1.0"; }
    
    /**
     * Validate that the source file can be imported
     * @param sourcePath Path to validate
     * @return true if file is valid for import
     */
    virtual bool ValidateSource(const std::filesystem::path& sourcePath) const;
    
    /**
     * Get a human-readable description of this factory
     */
    virtual std::string GetDescription() const { return "Asset Factory"; }
    
    /**
     * Get the factory name for identification
     */
    virtual std::string GetFactoryName() const = 0;
    
    /**
     * Check if this factory supports reimport
     */
    virtual bool SupportsReimport() const { return true; }
    
    /**
     * Get the source file path for a reimport
     * @param assetId ID of the asset
     * @return Path to source file, empty if not found
     */
    virtual std::filesystem::path GetSourcePath(const AssetId& assetId) const;
    
protected:
    /**
     * Helper to check file extension against supported extensions
     */
    bool HasSupportedExtension(const std::filesystem::path& sourcePath) const;
};

/**
 * Factory for creating new assets (not importing from files)
 * Similar to UE5's asset creation factories
 */
class AssetCreationFactory : public AssetFactory {
public:
    /**
     * Create a new asset
     * @param assetName Name for the new asset
     * @param outputDir Directory where asset should be created
     * @param creationSettings JSON string with creation settings
     * @return Import result with the created asset info
     */
    virtual ImportResult Create(const std::string& assetName,
                               const std::filesystem::path& outputDir,
                               const std::string& creationSettings) = 0;
    
    /**
     * Check if this factory can create assets without source files
     */
    virtual bool CanCreateNew() const { return true; }
    
    // Override base class methods
    std::vector<std::string> GetSupportedExtensions() const override { return {}; }
    bool CanImport(const std::filesystem::path& sourcePath) const override { 
        (void)sourcePath;
        return false; 
    }
    ImportResult Import(const std::filesystem::path& sourcePath,
                       const std::string& importSettings,
                       const std::filesystem::path& outputDir) override;
};

/**
 * Reimport factory for handling asset reimports
 * Similar to UE5's ReimportFbxStaticMeshFactory pattern
 */
class ReimportFactory : public AssetFactory {
public:
    /**
     * Check if the asset needs reimport based on current state
     * @param assetId ID of the asset to check
     * @return true if reimport is needed
     */
    virtual bool NeedsReimport(const AssetId& assetId) const = 0;
    
    /**
     * Get the current import settings for an asset
     * @param assetId ID of the asset
     * @return Current import settings as JSON
     */
    virtual std::string GetCurrentSettings(const AssetId& assetId) const = 0;
    
    /**
     * Clean up old derived files before reimport
     * @param assetId ID of the asset
     */
    virtual void CleanupDerivedFiles(const AssetId& assetId);
};

/**
 * Factory registry for managing all available factories
 */
class FactoryRegistry {
public:
    static FactoryRegistry& Instance();
    
    /**
     * Register a factory
     */
    void RegisterFactory(std::shared_ptr<AssetFactory> factory);
    
    /**
     * Unregister a factory
     */
    void UnregisterFactory(const std::string& factoryName);
    
    /**
     * Get a factory by name
     */
    std::shared_ptr<AssetFactory> GetFactory(const std::string& factoryName) const;
    
    /**
     * Get the best factory for a given file
     * @param sourcePath Path to the source file
     * @return Best factory for the file, or nullptr if none found
     */
    std::shared_ptr<AssetFactory> GetFactoryForFile(const std::filesystem::path& sourcePath) const;
    
    /**
     * Get all factories for a specific asset type
     */
    std::vector<std::shared_ptr<AssetFactory>> GetFactoriesForType(AssetType type) const;
    
    /**
     * Get all registered factories
     */
    const std::vector<std::shared_ptr<AssetFactory>>& GetAllFactories() const;
    
    /**
     * Get all creation factories (factories that can create new assets)
     */
    std::vector<std::shared_ptr<AssetCreationFactory>> GetCreationFactories() const;
    
    /**
     * Get all reimport factories
     */
    std::vector<std::shared_ptr<ReimportFactory>> GetReimportFactories() const;
    
private:
    FactoryRegistry() = default;
    
    std::vector<std::shared_ptr<AssetFactory>> m_factories;
    mutable std::mutex m_mutex;
};

} // namespace Luma