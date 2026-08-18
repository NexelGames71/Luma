#include "Luma/Asset/ImportDialog.h"
#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Asset/AssetImportManager.h"
#include "Luma/Asset/AssetMetadata.h"
#include "Luma/Asset/MeshImporter.h"
#include "Luma/Asset/TextureImporter.h"
#include "Luma/Core/Log.h"
#include <iostream>
#include <algorithm>

namespace Luma {

// ============================================================================
// MeshImportDialog Implementation
// ============================================================================

MeshImportDialog::MeshImportDialog() {
    LUMA_LOG_INFO("MeshImportDialog", "Initialized");
}

MeshImportDialog::~MeshImportDialog() {
    LUMA_LOG_INFO("MeshImportDialog", "Shutdown");
}

ImportDialogResult MeshImportDialog::Show(const ImportDialogConfig& config) {
    // For now, use console-based dialog
    // In production, this would show an actual UI dialog
    
    std::cout << "\n=== " << GetDialogTitle() << " ===\n";
    std::cout << "Source: " << config.sourcePath << "\n";
    std::cout << "Output: " << config.outputDirectory << "\n\n";
    
    return ShowConsoleDialog(config);
}

ImportDialogResult MeshImportDialog::ShowConsoleDialog(const ImportDialogConfig& config) {
    ImportDialogResult result;
    UIState state;
    
    // Parse existing settings if provided
    if (!config.importSettings.empty()) {
        // For simplicity, we'll use default settings
        // In production, parse the JSON properly
    }
    
    std::cout << "Mesh Import Settings:\n";
    std::cout << "1. Generate Normals: " << (state.generateNormals ? "Yes" : "No") << "\n";
    std::cout << "2. Generate Tangents: " << (state.generateTangents ? "Yes" : "No") << "\n";
    std::cout << "3. Flip UVs: " << (state.flipUVs ? "Yes" : "No") << "\n";
    std::cout << "4. Optimize Mesh: " << (state.optimizeMesh ? "Yes" : "No") << "\n";
    std::cout << "5. Scale: " << state.scale << "\n";
    std::cout << "6. Import Skeleton: " << (state.importSkeleton ? "Yes" : "No") << "\n";
    std::cout << "7. Import Animations: " << (state.importAnimations ? "Yes" : "No") << "\n";
    std::cout << "\n";
    
    std::cout << "Options:\n";
    std::cout << "I - Import with current settings\n";
    std::cout << "C - Cancel\n";
    std::cout << "D - Use default settings\n";
    
    std::cout << "\nChoice: ";
    std::string choice;
    std::getline(std::cin, choice);
    
    std::transform(choice.begin(), choice.end(), choice.begin(), ::tolower);
    
    if (choice == "i" || choice == "import") {
        result.confirmed = true;
        
        // Build settings JSON
        MeshImportSettings settings;
        settings.generateNormals = state.generateNormals;
        settings.generateTangents = state.generateTangents;
        settings.flipUVs = state.flipUVs;
        settings.optimizeMesh = state.optimizeMesh;
        settings.optimizeVertexCache = state.optimizeVertexCache;
        settings.optimizeVertexFetch = state.optimizeVertexFetch;
        settings.scale = state.scale;
        settings.importSkeleton = state.importSkeleton;
        settings.importAnimations = state.importAnimations;
        
        result.importSettings = settings.ToJson();
        result.outputDirectory = config.outputDirectory;
        
    } else if (choice == "d" || choice == "default") {
        result.confirmed = true;
        result.importSettings = MeshImportSettings::GetDefaults().ToJson();
        result.outputDirectory = config.outputDirectory;
    } else {
        result.confirmed = false;
    }
    
    return result;
}

// ============================================================================
// TextureImportDialog Implementation
// ============================================================================

TextureImportDialog::TextureImportDialog() {
    LUMA_LOG_INFO("TextureImportDialog", "Initialized");
}

TextureImportDialog::~TextureImportDialog() {
    LUMA_LOG_INFO("TextureImportDialog", "Shutdown");
}

ImportDialogResult TextureImportDialog::Show(const ImportDialogConfig& config) {
    std::cout << "\n=== " << GetDialogTitle() << " ===\n";
    std::cout << "Source: " << config.sourcePath << "\n";
    std::cout << "Output: " << config.outputDirectory << "\n\n";
    
    return ShowConsoleDialog(config);
}

ImportDialogResult TextureImportDialog::ShowConsoleDialog(const ImportDialogConfig& config) {
    ImportDialogResult result;
    UIState state;
    
    std::cout << "Texture Import Settings:\n";
    std::cout << "1. Generate Mipmaps: " << (state.generateMipmaps ? "Yes" : "No") << "\n";
    std::cout << "2. sRGB: " << (state.sRGB ? "Yes" : "No") << "\n";
    std::cout << "3. Compress: " << (state.compress ? "Yes" : "No") << "\n";
    std::cout << "4. Normal Map: " << (state.normalMap ? "Yes" : "No") << "\n";
    std::cout << "5. Max Size: " << state.maxTextureSize << "\n";
    std::cout << "6. Scale: " << state.scale << "\n";
    std::cout << "7. Flip Y: " << (state.flipY ? "Yes" : "No") << "\n";
    std::cout << "\n";
    
    std::cout << "Options:\n";
    std::cout << "I - Import with current settings\n";
    std::cout << "C - Cancel\n";
    std::cout << "D - Use default settings\n";
    
    std::cout << "\nChoice: ";
    std::string choice;
    std::getline(std::cin, choice);
    
    std::transform(choice.begin(), choice.end(), choice.begin(), ::tolower);
    
    if (choice == "i" || choice == "import") {
        result.confirmed = true;
        
        TextureImportSettings settings;
        settings.generateMipmaps = state.generateMipmaps;
        settings.sRGB = state.sRGB;
        settings.compress = state.compress;
        settings.normalMap = state.normalMap;
        settings.maxTextureSize = state.maxTextureSize;
        settings.scale = state.scale;
        settings.flipY = state.flipY;
        
        result.importSettings = settings.ToJson();
        result.outputDirectory = config.outputDirectory;
        
    } else if (choice == "d" || choice == "default") {
        result.confirmed = true;
        result.importSettings = TextureImportSettings::GetDefaults().ToJson();
        result.outputDirectory = config.outputDirectory;
    } else {
        result.confirmed = false;
    }
    
    return result;
}

// ============================================================================
// ImportDialogManager Implementation
// ============================================================================

ImportDialogManager& ImportDialogManager::Instance() {
    static ImportDialogManager instance;
    return instance;
}

void ImportDialogManager::RegisterDialog(std::shared_ptr<ImportDialog> dialog) {
    if (!dialog) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if dialog for this type already exists
    auto it = std::remove_if(m_dialogs.begin(), m_dialogs.end(),
        [&dialog](const std::shared_ptr<ImportDialog>& d) {
            return d->GetAssetType() == dialog->GetAssetType();
        });
    
    m_dialogs.erase(it, m_dialogs.end());
    m_dialogs.push_back(dialog);
    
    LUMA_LOG_INFO("ImportDialogManager", "Registered dialog for type: {}", 
                 static_cast<int>(dialog->GetAssetType()));
}

void ImportDialogManager::UnregisterDialog(AssetType assetType) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::remove_if(m_dialogs.begin(), m_dialogs.end(),
        [assetType](const std::shared_ptr<ImportDialog>& d) {
            return d->GetAssetType() == assetType;
        });
    
    m_dialogs.erase(it, m_dialogs.end());
    
    LUMA_LOG_INFO("ImportDialogManager", "Unregistered dialog for type: {}", 
                 static_cast<int>(assetType));
}

std::shared_ptr<ImportDialog> ImportDialogManager::GetDialog(AssetType assetType) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& dialog : m_dialogs) {
        if (dialog->GetAssetType() == assetType) {
            return dialog;
        }
    }
    
    return nullptr;
}

std::optional<ImportDialogResult> ImportDialogManager::ShowImportDialog(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& outputDir,
    AssetRegistry* registry) {
    
    // Determine asset type from file extension
    AssetType assetType = AssetType::Unknown;
    std::string ext = sourcePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Simple extension mapping
    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" || 
        ext == ".dae" || ext == ".blend") {
        assetType = AssetType::Mesh;
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || 
               ext == ".bmp" || ext == ".psd") {
        assetType = AssetType::Texture;
    }
    
    if (assetType == AssetType::Unknown) {
        LUMA_LOG_WARN("ImportDialogManager", "Unknown asset type for file: {}", sourcePath.string());
        return std::nullopt;
    }
    
    auto dialog = GetDialog(assetType);
    if (!dialog) {
        LUMA_LOG_WARN("ImportDialogManager", "No dialog registered for asset type: {}", 
                        static_cast<int>(assetType));
        return std::nullopt;
    }
    
    ImportDialogConfig config;
    config.assetType = assetType;
    config.sourcePath = sourcePath.string();
    config.outputDirectory = outputDir.string();
    config.enableReimport = false;
    
    return dialog->Show(config);
}

std::optional<ImportDialogResult> ImportDialogManager::ShowReimportDialog(
    const AssetId& assetId,
    AssetRegistry* registry) {
    
    if (!registry) {
        LUMA_LOG_ERROR("ImportDialogManager", "Asset registry is null for reimport");
        return std::nullopt;
    }
    
    auto assetData = registry->Lookup(assetId);
    if (!assetData) {
        LUMA_LOG_ERROR("ImportDialogManager", "Asset not found for reimport");
        return std::nullopt;
    }
    
    // TODO: Implement when metadata system is fully integrated
    (void)assetId;
    (void)registry;
    LUMA_LOG_WARN("ImportDialogManager", "Reimport dialog not yet implemented");
    return std::nullopt;
}

std::optional<ImportDialogResult> ImportDialogManager::ShowBatchImportDialog(
    const std::vector<std::filesystem::path>& sourcePaths,
    const std::filesystem::path& outputDir,
    AssetRegistry* registry) {
    
    if (sourcePaths.empty()) {
        LUMA_LOG_WARN("ImportDialogManager", "No files provided for batch import");
        return std::nullopt;
    }
    
    // Determine common asset type from first file
    auto firstExt = sourcePaths[0].extension().string();
    std::transform(firstExt.begin(), firstExt.end(), firstExt.begin(), ::tolower);
    
    AssetType commonType = AssetType::Unknown;
    if (firstExt == ".fbx" || firstExt == ".obj" || firstExt == ".gltf" || firstExt == ".glb") {
        commonType = AssetType::Mesh;
    } else if (firstExt == ".png" || firstExt == ".jpg" || firstExt == ".jpeg" || firstExt == ".tga") {
        commonType = AssetType::Texture;
    }
    
    if (commonType == AssetType::Unknown) {
        LUMA_LOG_WARN("ImportDialogManager", "Cannot determine common asset type for batch import");
        return std::nullopt;
    }
    
    auto dialog = GetDialog(commonType);
    if (!dialog || !dialog->SupportsBatchImport()) {
        LUMA_LOG_WARN("ImportDialogManager", "Batch import not supported for this asset type");
        return std::nullopt;
    }
    
    std::cout << "\n=== Batch Import (" << sourcePaths.size() << " files) ===\n";
    for (const auto& path : sourcePaths) {
        std::cout << "  - " << path.filename().string() << "\n";
    }
    std::cout << "\n";
    
    ImportDialogConfig config;
    config.assetType = commonType;
    config.sourcePath = sourcePaths[0].string(); // Use first file as example
    config.outputDirectory = outputDir.string();
    config.enableReimport = false;
    
    auto result = dialog->Show(config);
    result.applyToAll = true; // Mark as batch import
    
    return result;
}

// ============================================================================
// ImportDialogHelper Implementation
// ============================================================================

bool ImportDialogHelper::ShowAndImport(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& outputDir,
    AssetRegistry* registry,
    AssetImportManager* importManager) {
    
    if (!importManager) {
        LUMA_LOG_ERROR("ImportDialogHelper", "Import manager is null");
        return false;
    }
    
    auto& dialogManager = ImportDialogManager::Instance();
    auto dialogResult = dialogManager.ShowImportDialog(sourcePath, outputDir, registry);
    
    if (!dialogResult.has_value() || !dialogResult.value().confirmed) {
        return false;
    }
    
    const auto& result = dialogResult.value();
    
    // Determine asset type
    AssetType assetType = AssetType::Unknown;
    std::string ext = sourcePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb") {
        assetType = AssetType::Mesh;
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
        assetType = AssetType::Texture;
    }
    
    if (assetType == AssetType::Unknown) {
        LUMA_LOG_ERROR("ImportDialogHelper", "Unknown asset type");
        return false;
    }
    
    // Queue the import
    auto jobId = importManager->QueueImport(
        sourcePath,
        assetType,
        result.importSettings,
        nullptr); // No callback for now
    
    if (jobId.IsValid()) {
        LUMA_LOG_INFO("ImportDialogHelper", "Import queued successfully: {}", Luma::ToString(jobId));
        
        // Save settings as global defaults if requested
        if (result.rememberSettings) {
            importManager->GetGlobalSettings() = GlobalImportSettings::FromJson(result.importSettings);
        }
        
        return true;
    }
    
    return false;
}

bool ImportDialogHelper::ShowAndReimport(
    const AssetId& assetId,
    AssetRegistry* registry,
    AssetImportManager* importManager) {
    
    if (!importManager || !registry) {
        LUMA_LOG_ERROR("ImportDialogHelper", "Import manager or registry is null");
        return false;
    }
    
    auto& dialogManager = ImportDialogManager::Instance();
    auto dialogResult = dialogManager.ShowReimportDialog(assetId, registry);
    
    if (!dialogResult.has_value() || !dialogResult.value().confirmed) {
        return false;
    }
    
    const auto& result = dialogResult.value();
    
    // Queue the reimport
    auto jobId = importManager->QueueReimport(
        assetId,
        nullptr); // No callback for now
    
    if (jobId.IsValid()) {
        LUMA_LOG_INFO("ImportDialogHelper", "Reimport queued successfully: {}", Luma::ToString(jobId));
        return true;
    }
    
    return false;
}

} // namespace Luma