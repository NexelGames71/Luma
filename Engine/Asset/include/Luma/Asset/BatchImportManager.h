#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <atomic>
#include <mutex>

#include "Luma/Asset/AssetId.h"
#include "Luma/Asset/AssetType.h"
#include "Luma/Asset/AssetImportManager.h"

namespace Luma {

// Forward declarations
class AssetRegistry;

/**
 * Batch import job information
 */
struct BatchImportJob {
    std::filesystem::path sourcePath;
    AssetType assetType = AssetType::Unknown;
    std::string importSettings;
    AssetId jobId;
    ImportStatus status = ImportStatus::Pending;
    std::string errorMessage;
};

/**
 * Batch import result summary
 */
struct BatchImportResult {
    usize totalFiles = 0;
    usize successfulImports = 0;
    usize failedImports = 0;
    usize skippedImports = 0;
    std::vector<BatchImportJob> jobs;
    double totalTimeSeconds = 0.0;
    
    bool IsComplete() const {
        return successfulImports + failedImports + skippedImports == totalFiles;
    }
    
    f32 GetSuccessRate() const {
        if (totalFiles == 0) return 0.0f;
        return static_cast<f32>(successfulImports) / static_cast<f32>(totalFiles);
    }
};

/**
 * Progress callback for batch import operations
 */
using BatchImportProgressCallback = std::function<void(
    const BatchImportJob& currentJob,
    usize completedCount,
    usize totalCount,
    const BatchImportResult& result)>;

/**
 * Completion callback for batch import operations
 */
using BatchImportCompletionCallback = std::function<void(const BatchImportResult& result)>;

/**
 * Batch import configuration
 */
struct BatchImportConfig {
    std::filesystem::path outputDirectory;
    std::string commonImportSettings;  // Settings applied to all files
    bool useCommonSettings = true;     // Use same settings for all files
    bool skipExisting = false;         // Skip if native file already exists
    bool continueOnError = true;      // Continue importing even if some files fail
    usize maxConcurrentImports = 4;    // Maximum concurrent imports
    bool showProgressDialog = true;    // Show progress dialog during import
    bool generateLog = true;           // Generate import log file
    std::filesystem::path logFilePath; // Path for log file
};

/**
 * Batch Import Manager - handles importing multiple assets at once
 * Inspired by UE5's FAssetImportManager::ImportAssets functionality
 */
class BatchImportManager {
public:
    BatchImportManager();
    ~BatchImportManager();
    
    /**
     * Initialize the batch import manager
     */
    void Initialize(AssetRegistry* registry, AssetImportManager* importManager);
    
    /**
     * Shutdown and wait for all batch jobs to complete
     */
    void Shutdown();
    
    /**
     * Import multiple files with default settings
     * @param sourcePaths Paths to source files
     * @param outputDir Output directory
     * @param config Batch import configuration
     * @param progressCallback Optional progress callback
     * @param completionCallback Optional completion callback
     * @return Batch job ID for tracking
     */
    AssetId ImportFiles(
        const std::vector<std::filesystem::path>& sourcePaths,
        const std::filesystem::path& outputDir,
        const BatchImportConfig& config = BatchImportConfig(),
        BatchImportProgressCallback progressCallback = nullptr,
        BatchImportCompletionCallback completionCallback = nullptr);
    
    /**
     * Import all files from a directory
     * @param directory Directory to scan for files
     * @param outputDir Output directory
     * @param config Batch import configuration
     * @param progressCallback Optional progress callback
     * @param completionCallback Optional completion callback
     * @return Batch job ID for tracking
     */
    AssetId ImportDirectory(
        const std::filesystem::path& directory,
        const std::filesystem::path& outputDir,
        const BatchImportConfig& config = BatchImportConfig(),
        BatchImportProgressCallback progressCallback = nullptr,
        BatchImportCompletionCallback completionCallback = nullptr);
    
    /**
     * Import files matching a pattern
     * @param directory Directory to scan
     * @param pattern File pattern (e.g., "*.fbx")
     * @param outputDir Output directory
     * @param config Batch import configuration
     * @param progressCallback Optional progress callback
     * @param completionCallback Optional completion callback
     * @return Batch job ID for tracking
     */
    AssetId ImportPattern(
        const std::filesystem::path& directory,
        const std::string& pattern,
        const std::filesystem::path& outputDir,
        const BatchImportConfig& config = BatchImportConfig(),
        BatchImportProgressCallback progressCallback = nullptr,
        BatchImportCompletionCallback completionCallback = nullptr);
    
    /**
     * Cancel a batch import job
     */
    void CancelBatchJob(const AssetId& batchJobId);
    
    /**
     * Get the status of a batch import job
     */
    BatchImportResult GetBatchJobStatus(const AssetId& batchJobId) const;
    
    /**
     * Check if a batch job is complete
     */
    bool IsBatchJobComplete(const AssetId& batchJobId) const;
    
    /**
     * Process completed batch jobs (call from main thread)
     */
    void ProcessCompletedBatchJobs();
    
    /**
     * Get the number of active batch jobs
     */
    usize GetActiveBatchJobCount() const;
    
    /**
     * Generate an import log file for a batch job
     */
    bool GenerateImportLog(const AssetId& batchJobId, const std::filesystem::path& logPath);
    
private:
    /**
     * Process a single batch import job
     */
    void ProcessBatchJob(AssetId batchJobId);
    
    /**
     * Import a single file within a batch job
     */
    void ImportSingleFile(BatchImportJob& job, 
                         const BatchImportConfig& config,
                         AssetImportManager* importManager);
    
    /**
     * Determine asset type from file extension
     */
    AssetType DetermineAssetType(const std::filesystem::path& filePath) const;
    
    /**
     * Check if file should be skipped
     */
    bool ShouldSkipFile(const std::filesystem::path& sourcePath,
                       const std::filesystem::path& outputDir,
                       const BatchImportConfig& config) const;
    
    /**
     * Group files by asset type for optimized processing
     */
    std::unordered_map<AssetType, std::vector<std::filesystem::path>> 
    GroupFilesByType(const std::vector<std::filesystem::path>& files) const;
    
    /**
     * Update batch job progress
     */
    void UpdateProgress(const AssetId& batchJobId, 
                       const BatchImportJob& currentJob,
                       usize completedCount);
    
    /**
     * Complete a batch job
     */
    void CompleteBatchJob(AssetId batchJobId, BatchImportResult result);
    
    // Asset registry and import manager
    AssetRegistry* m_registry = nullptr;
    AssetImportManager* m_importManager = nullptr;
    
    // Batch job tracking
    struct BatchJob {
        AssetId jobId;
        BatchImportResult result;
        BatchImportConfig config;
        BatchImportProgressCallback progressCallback;
        BatchImportCompletionCallback completionCallback;
        std::atomic<bool> cancelled{false};
        std::atomic<bool> processing{false};
    };
    
    std::unordered_map<AssetId, std::shared_ptr<BatchJob>> m_batchJobs;
    mutable std::mutex m_batchJobsMutex;
    
    // Completed batch jobs (for main thread callbacks)
    std::vector<std::pair<AssetId, BatchImportResult>> m_completedBatchJobs;
    mutable std::mutex m_completedBatchJobsMutex;
    
    // Background thread for batch processing
    std::thread m_batchThread;
    std::atomic<bool> m_shutdown{false};
    std::condition_variable m_batchThreadCV;
    std::mutex m_batchThreadMutex;
    std::queue<AssetId> m_batchJobQueue;
};

/**
 * Helper class for common batch import scenarios
 */
class BatchImportHelper {
public:
    /**
     * Quick import of multiple files with sensible defaults
     */
    static AssetId QuickImport(
        const std::vector<std::filesystem::path>& sourcePaths,
        const std::filesystem::path& outputDir,
        AssetRegistry* registry,
        AssetImportManager* importManager);
    
    /**
     * Import an entire directory structure
     */
    static AssetId ImportDirectoryTree(
        const std::filesystem::path& sourceDir,
        const std::filesystem::path& outputDir,
        AssetRegistry* registry,
        AssetImportManager* importManager,
        bool preserveStructure = true);
    
    /**
     * Reimport all assets that need reimporting
     */
    static AssetId ReimportAllOutdated(
        AssetRegistry* registry,
        AssetImportManager* importManager);
    
    /**
     * Create a batch import config from JSON settings
     */
    static BatchImportConfig ConfigFromSettings(
        const std::string& importSettings,
        const std::filesystem::path& outputDir);
};

} // namespace Luma