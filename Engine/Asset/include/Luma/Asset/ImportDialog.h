#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>

#include "Luma/Asset/AssetType.h"
#include "Luma/Asset/AssetId.h"

namespace Luma {

// Forward declarations
class AssetRegistry;
class AssetImportManager;

/**
 * Import dialog configuration for different asset types
 * Similar to UE5's import dialogs (FbxImportUI, TextureImportUI, etc.)
 */
struct ImportDialogConfig {
    AssetType assetType = AssetType::Unknown;
    std::string factoryName;
    std::string sourcePath;
    std::string outputDirectory;
    std::string importSettings;  // JSON string with current settings
    bool showAdvancedOptions = false;
    bool enableReimport = false;
    AssetId reimportAssetId;  // Set if this is a reimport dialog
};

/**
 * Import dialog result containing user choices
 */
struct ImportDialogResult {
    bool confirmed = false;
    std::string importSettings;  // Final settings chosen by user
    std::string outputDirectory;
    bool rememberSettings = false;  // Save as global defaults
    bool applyToAll = false;       // Apply to all selected files (batch import)
};

/**
 * Base class for import dialogs
 * Provides the interface for asset-specific import UI
 */
class ImportDialog {
public:
    virtual ~ImportDialog() = default;
    
    /**
     * Show the import dialog and get user input
     * @param config Dialog configuration
     * @return Result with user choices
     */
    virtual ImportDialogResult Show(const ImportDialogConfig& config) = 0;
    
    /**
     * Get the asset type this dialog handles
     */
    virtual AssetType GetAssetType() const = 0;
    
    /**
     * Get the dialog title
     */
    virtual std::string GetDialogTitle() const = 0;
    
    /**
     * Check if this dialog supports reimport
     */
    virtual bool SupportsReimport() const { return true; }
    
    /**
     * Check if this dialog supports batch import
     */
    virtual bool SupportsBatchImport() const { return true; }
};

/**
 * Mesh import dialog for configuring mesh import settings
 * Similar to UE5's FBX Import dialog
 */
class MeshImportDialog : public ImportDialog {
public:
    MeshImportDialog();
    ~MeshImportDialog() override;
    
    ImportDialogResult Show(const ImportDialogConfig& config) override;
    AssetType GetAssetType() const override { return AssetType::Mesh; }
    std::string GetDialogTitle() const override { return "Import Mesh"; }
    
private:
    struct UIState {
        bool generateNormals = true;
        bool generateTangents = true;
        bool flipUVs = false;
        bool optimizeMesh = true;
        bool optimizeVertexCache = true;
        bool optimizeVertexFetch = true;
        f32 scale = 1.0f;
        bool importSkeleton = true;
        bool importAnimations = true;
        bool showAdvanced = false;
    };
    
    // In a real implementation, this would show an actual UI dialog
    // For now, we provide a console-based version
    ImportDialogResult ShowConsoleDialog(const ImportDialogConfig& config);
};

/**
 * Texture import dialog for configuring texture import settings
 * Similar to UE5's Texture Import dialog
 */
class TextureImportDialog : public ImportDialog {
public:
    TextureImportDialog();
    ~TextureImportDialog() override;
    
    ImportDialogResult Show(const ImportDialogConfig& config) override;
    AssetType GetAssetType() const override { return AssetType::Texture; }
    std::string GetDialogTitle() const override { return "Import Texture"; }
    
private:
    struct UIState {
        bool generateMipmaps = true;
        bool sRGB = true;
        bool compress = true;
        bool normalMap = false;
        i32 maxTextureSize = 4096;
        i32 desiredSize = 0;
        f32 scale = 1.0f;
        bool preserveAlpha = true;
        bool flipY = false;
        bool showAdvanced = false;
    };
    
    ImportDialogResult ShowConsoleDialog(const ImportDialogConfig& config);
};

/**
 * Import dialog manager for handling all import dialogs
 * Similar to UE5's dialog management system
 */
class ImportDialogManager {
public:
    static ImportDialogManager& Instance();
    
    /**
     * Register an import dialog for a specific asset type
     */
    void RegisterDialog(std::shared_ptr<ImportDialog> dialog);
    
    /**
     * Unregister a dialog
     */
    void UnregisterDialog(AssetType assetType);
    
    /**
     * Get the dialog for a specific asset type
     */
    std::shared_ptr<ImportDialog> GetDialog(AssetType assetType) const;
    
    /**
     * Show the appropriate import dialog for a file
     * @param sourcePath Path to the source file
     * @param outputDir Default output directory
     * @param registry Asset registry for reimport detection
     * @return Result with user choices, or empty if cancelled
     */
    std::optional<ImportDialogResult> ShowImportDialog(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& outputDir,
        AssetRegistry* registry = nullptr);
    
    /**
     * Show a reimport dialog for an existing asset
     * @param assetId ID of the asset to reimport
     * @param registry Asset registry
     * @return Result with user choices, or empty if cancelled
     */
    std::optional<ImportDialogResult> ShowReimportDialog(
        const AssetId& assetId,
        AssetRegistry* registry);
    
    /**
     * Show a batch import dialog for multiple files
     * @param sourcePaths Paths to the source files
     * @param outputDir Default output directory
     * @param registry Asset registry
     * @return Result with user choices, or empty if cancelled
     */
    std::optional<ImportDialogResult> ShowBatchImportDialog(
        const std::vector<std::filesystem::path>& sourcePaths,
        const std::filesystem::path& outputDir,
        AssetRegistry* registry = nullptr);
    
private:
    ImportDialogManager() = default;
    
    std::vector<std::shared_ptr<ImportDialog>> m_dialogs;
    mutable std::mutex m_mutex;
};

/**
 * Helper function to show import dialog and execute import
 * Combines dialog showing with import execution
 */
struct ImportDialogHelper {
    /**
     * Show import dialog and execute import if confirmed
     * @param sourcePath Path to source file
     * @param outputDir Output directory
     * @param registry Asset registry
     * @param importManager Import manager
     * @return true if import was queued, false if cancelled or failed
     */
    static bool ShowAndImport(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& outputDir,
        AssetRegistry* registry,
        AssetImportManager* importManager);
    
    /**
     * Show reimport dialog and execute reimport if confirmed
     * @param assetId ID of asset to reimport
     * @param registry Asset registry
     * @param importManager Import manager
     * @return true if reimport was queued, false if cancelled or failed
     */
    static bool ShowAndReimport(
        const AssetId& assetId,
        AssetRegistry* registry,
        AssetImportManager* importManager);
};

} // namespace Luma