#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <condition_variable>

#include "Luma/Asset/AssetId.h"
#include "Luma/Asset/AssetMetadata.h"
#include "Luma/Asset/AssetType.h"
#include "Luma/Core/Types.h"

// Forward declarations
namespace Luma {
class AssetRegistry;
}

namespace Luma {

// Import job status and result
enum class ImportStatus {
    Pending,
    InProgress,
    Completed,
    Failed,
    Cancelled,
    Success  // Alias for Completed for clarity
};

struct ImportResult {
    ImportStatus status = ImportStatus::Pending;
    std::string errorMessage;
    AssetId assetId{};
    std::filesystem::path sourcePath;
    std::filesystem::path nativePath;
    std::filesystem::path outputPath;
    i64 importTime = 0;
};

// Import job description
struct ImportJob {
    AssetId jobId{};
    std::filesystem::path sourcePath;
    AssetType assetType = AssetType::Unknown;
    bool isReimport = false;
    std::string importSettings;  // JSON string of type-specific settings
    
    // Callback for completion (called on main thread)
    std::function<void(const ImportResult&)> onComplete;
};

// Abstract base class for asset importers
class IAssetImporter {
public:
    virtual ~IAssetImporter() = default;
    
    // Returns the asset type this importer handles
    virtual AssetType GetAssetType() const = 0;
    
    // Returns the importer version string (for detecting when importer changes)
    virtual std::string GetImporterVersion() const = 0;
    
    // Check if this importer can handle the given file extension
    virtual bool CanImport(const std::filesystem::path& sourcePath) const = 0;
    
    // Import the asset. Called on worker thread.
    // Returns ImportResult with status, error message, and generated native path.
    virtual ImportResult Import(const std::filesystem::path& sourcePath,
                                const std::string& importSettings,
                                const std::filesystem::path& outputDir) = 0;
    
    // Get default import settings for this importer type
    virtual std::string GetDefaultSettings() const = 0;
};

// Global import settings for each asset type
struct GlobalImportSettings {
    MeshImportSettings mesh;
    TextureImportSettings texture;  // Texture settings now enabled
    // Future: audio, material, etc. settings
    
    // Serialize to/from JSON
    std::string ToJson() const;
    static GlobalImportSettings FromJson(const std::string& json);
    static GlobalImportSettings GetDefaults();
};

// Asset Import Manager - central coordinator for asset importing
// Manages import jobs, worker threads, and importer plugins.
class AssetImportManager {
public:
    AssetImportManager();
    ~AssetImportManager();
    
    // Initialize the import manager with an asset registry
    void Initialize(AssetRegistry* registry);
    
    // Shutdown and wait for all jobs to complete
    void Shutdown();
    
    // Register an importer plugin for a specific asset type
    void RegisterImporter(std::unique_ptr<IAssetImporter> importer);
    
    // Queue an import job. Returns the job ID.
    AssetId QueueImport(const std::filesystem::path& sourcePath, 
                       AssetType type,
                       const std::string& importSettings = "",
                       std::function<void(const ImportResult&)> onComplete = nullptr);
    
    // Queue a reimport job for an existing asset
    AssetId QueueReimport(const AssetId& assetId,
                         std::function<void(const ImportResult&)> onComplete = nullptr);
    
    // Cancel an import job
    void CancelImport(const AssetId& jobId);
    
    // Get the status of an import job
    ImportStatus GetJobStatus(const AssetId& jobId) const;
    
    // Get the result of a completed job (empty if not completed)
    std::optional<ImportResult> GetJobResult(const AssetId& jobId) const;
    
    // Process completed jobs and call their callbacks (call from main thread)
    void ProcessCompletedJobs();
    
    // Get global import settings
    const GlobalImportSettings& GetGlobalSettings() const { return m_globalSettings; }
    GlobalImportSettings& GetGlobalSettings() { return m_globalSettings; }
    
    // Save/load global import settings
    bool SaveGlobalSettings(const std::filesystem::path& path);
    bool LoadGlobalSettings(const std::filesystem::path& path);
    
    // Check if an asset needs reimport based on metadata
    bool NeedsReimport(const AssetMetadata& meta) const;
    
    // Get the importer for a specific asset type
    IAssetImporter* GetImporter(AssetType type) const;
    
    // Number of active jobs
    usize GetActiveJobCount() const;

private:
    // Worker thread function
    void WorkerThread();
    
    // Process a single import job
    void ProcessJob(ImportJob& job);
    
    // Move completed job to main thread queue
    void CompleteJob(ImportJob&& job, ImportResult result);
    
    // Asset registry for metadata and asset tracking
    AssetRegistry* m_registry = nullptr;
    
    // Registered importers by asset type
    std::vector<std::unique_ptr<IAssetImporter>> m_importers;
    
    // Import job queue and worker threads
    std::queue<ImportJob> m_jobQueue;
    mutable std::mutex m_jobQueueMutex;
    std::condition_variable m_jobQueueCV;
    std::vector<std::thread> m_workerThreads;
    bool m_shutdown = false;
    
    // Completed jobs (for main thread processing)
    std::vector<std::pair<ImportJob, ImportResult>> m_completedJobs;
    mutable std::mutex m_completedJobsMutex;
    
    // Job tracking (status and results)
    std::unordered_map<AssetId, ImportResult> m_jobResults;
    mutable std::mutex m_jobResultsMutex;
    
    // Global import settings
    GlobalImportSettings m_globalSettings;
    
    // Number of worker threads
    static constexpr usize kWorkerThreadCount = 2;
};

// Singleton accessor
AssetImportManager& Global();

}  // namespace Luma
