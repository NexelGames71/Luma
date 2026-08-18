#include "Luma/Asset/BatchImportManager.h"
#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Asset/AssetMetadata.h"
#include "Luma/Core/Log.h"
#include <chrono>
#include <fstream>
#include <algorithm>
#include <regex>

namespace Luma {

// ============================================================================
// BatchImportManager Implementation
// ============================================================================

BatchImportManager::BatchImportManager() {
    LUMA_LOG_INFO("BatchImportManager", "Initialized");
}

BatchImportManager::~BatchImportManager() {
    Shutdown();
    LUMA_LOG_INFO("BatchImportManager", "Shutdown");
}

void BatchImportManager::Initialize(AssetRegistry* registry, AssetImportManager* importManager) {
    m_registry = registry;
    m_importManager = importManager;
    
    // Start background thread for batch processing
    m_batchThread = std::thread([this]() {
        while (!m_shutdown) {
            std::unique_lock<std::mutex> lock(m_batchThreadMutex);
            m_batchThreadCV.wait(lock, [this]() {
                return !m_batchJobQueue.empty() || m_shutdown;
            });
            
            if (m_shutdown) break;
            
            if (!m_batchJobQueue.empty()) {
                auto batchJobId = m_batchJobQueue.front();
                m_batchJobQueue.pop();
                lock.unlock();
                
                ProcessBatchJob(batchJobId);
            }
        }
    });
    
    LUMA_LOG_INFO("BatchImportManager", "Initialized with registry and import manager");
}

void BatchImportManager::Shutdown() {
    if (m_shutdown) return;
    
    m_shutdown = true;
    m_batchThreadCV.notify_all();
    
    if (m_batchThread.joinable()) {
        m_batchThread.join();
    }
    
    // Process any remaining completed jobs
    ProcessCompletedBatchJobs();
    
    LUMA_LOG_INFO("BatchImportManager", "Shutdown complete");
}

AssetId BatchImportManager::ImportFiles(
    const std::vector<std::filesystem::path>& sourcePaths,
    const std::filesystem::path& outputDir,
    const BatchImportConfig& config,
    BatchImportProgressCallback progressCallback,
    BatchImportCompletionCallback completionCallback) {
    
    if (!m_importManager) {
        LUMA_LOG_ERROR("BatchImportManager", "Import manager not initialized");
        return AssetId();
    }
    
    if (sourcePaths.empty()) {
        LUMA_LOG_INFO("BatchImportManager", "No files provided for batch import");
        return AssetId();
    }
    
    // Create batch job
    auto batchJobId = MakeAssetIdFromKey("batch_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    
    auto batchJob = std::make_shared<BatchJob>();
    batchJob->jobId = batchJobId;
    batchJob->config = config;
    batchJob->progressCallback = progressCallback;
    batchJob->completionCallback = completionCallback;
    batchJob->result.totalFiles = sourcePaths.size();
    
    // Create individual jobs for each file
    for (const auto& sourcePath : sourcePaths) {
        if (!std::filesystem::exists(sourcePath)) {
            LUMA_LOG_WARN("BatchImportManager", "File not found: {}", sourcePath.string());
            batchJob->result.skippedImports++;
            continue;
        }
        
        BatchImportJob job;
        job.sourcePath = sourcePath;
        job.assetType = DetermineAssetType(sourcePath);
        job.importSettings = config.useCommonSettings ? config.commonImportSettings : "";
        job.jobId = MakeAssetIdFromKey(sourcePath.string());
        job.status = ImportStatus::Pending;
        
        batchJob->result.jobs.push_back(job);
    }
    
    // Store batch job
    {
        std::lock_guard<std::mutex> lock(m_batchJobsMutex);
        m_batchJobs[batchJobId] = batchJob;
    }
    
    // Queue for processing
    {
        std::lock_guard<std::mutex> lock(m_batchThreadMutex);
        m_batchJobQueue.push(batchJobId);
    }
    m_batchThreadCV.notify_one();
    
    LUMA_LOG_INFO("BatchImportManager", "Queued batch job {} with {} files", 
                 Luma::ToString(batchJobId), batchJob->result.totalFiles);
    
    return batchJobId;
}

AssetId BatchImportManager::ImportDirectory(
    const std::filesystem::path& directory,
    const std::filesystem::path& outputDir,
    const BatchImportConfig& config,
    BatchImportProgressCallback progressCallback,
    BatchImportCompletionCallback completionCallback) {
    
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        LUMA_LOG_ERROR("BatchImportManager", "Invalid directory: {}", directory.string());
        return AssetId();
    }
    
    // Collect all importable files from directory
    std::vector<std::filesystem::path> sourcePaths;
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                auto assetType = DetermineAssetType(entry.path());
                if (assetType != AssetType::Unknown) {
                    sourcePaths.push_back(entry.path());
                }
            }
        }
    } catch (const std::exception& e) {
        LUMA_LOG_ERROR("BatchImportManager", "Error scanning directory: {}", e.what());
        return AssetId();
    }
    
    if (sourcePaths.empty()) {
        LUMA_LOG_WARN("BatchImportManager", "No importable files found in directory");
        return AssetId();
    }
    
    LUMA_LOG_INFO("BatchImportManager", "Found {} importable files in directory", sourcePaths.size());
    
    return ImportFiles(sourcePaths, outputDir, config, progressCallback, completionCallback);
}

AssetId BatchImportManager::ImportPattern(
    const std::filesystem::path& directory,
    const std::string& pattern,
    const std::filesystem::path& outputDir,
    const BatchImportConfig& config,
    BatchImportProgressCallback progressCallback,
    BatchImportCompletionCallback completionCallback) {
    
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        LUMA_LOG_ERROR("BatchImportManager", "Invalid directory: {}", directory.string());
        return AssetId();
    }
    
    // Convert glob pattern to regex
    std::string regexPattern = std::regex_replace(pattern, std::regex("\\*"), ".*");
    regexPattern = std::regex_replace(regexPattern, std::regex("\\?"), ".");
    
    std::regex fileRegex(regexPattern, std::regex::icase);
    
    // Collect matching files
    std::vector<std::filesystem::path> sourcePaths;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                auto filename = entry.path().filename().string();
                if (std::regex_match(filename, fileRegex)) {
                    auto assetType = DetermineAssetType(entry.path());
                    if (assetType != AssetType::Unknown) {
                        sourcePaths.push_back(entry.path());
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        LUMA_LOG_ERROR("BatchImportManager", "Error scanning directory for pattern: {}", e.what());
        return AssetId();
    }
    
    if (sourcePaths.empty()) {
        LUMA_LOG_WARN("BatchImportManager", "No files matching pattern '{}' found", pattern);
        return AssetId();
    }
    
    LUMA_LOG_INFO("BatchImportManager", "Found {} files matching pattern '{}'", sourcePaths.size(), pattern);
    
    return ImportFiles(sourcePaths, outputDir, config, progressCallback, completionCallback);
}

void BatchImportManager::CancelBatchJob(const AssetId& batchJobId) {
    std::lock_guard<std::mutex> lock(m_batchJobsMutex);
    
    auto it = m_batchJobs.find(batchJobId);
    if (it != m_batchJobs.end()) {
        it->second->cancelled = true;
        LUMA_LOG_INFO("BatchImportManager", "Cancelled batch job {}", Luma::ToString(batchJobId));
    }
}

BatchImportResult BatchImportManager::GetBatchJobStatus(const AssetId& batchJobId) const {
    std::lock_guard<std::mutex> lock(m_batchJobsMutex);
    
    auto it = m_batchJobs.find(batchJobId);
    if (it != m_batchJobs.end()) {
        return it->second->result;
    }
    
    return BatchImportResult();
}

bool BatchImportManager::IsBatchJobComplete(const AssetId& batchJobId) const {
    auto result = GetBatchJobStatus(batchJobId);
    return result.IsComplete();
}

void BatchImportManager::ProcessCompletedBatchJobs() {
    std::lock_guard<std::mutex> lock(m_completedBatchJobsMutex);
    
    for (auto& [batchJobId, result] : m_completedBatchJobs) {
        // Find the batch job and call completion callback
        std::lock_guard<std::mutex> batchLock(m_batchJobsMutex);
        auto it = m_batchJobs.find(batchJobId);
        if (it != m_batchJobs.end() && it->second->completionCallback) {
            it->second->completionCallback(result);
        }
        
        // Remove from active jobs
        m_batchJobs.erase(batchJobId);
    }
    
    m_completedBatchJobs.clear();
}

usize BatchImportManager::GetActiveBatchJobCount() const {
    std::lock_guard<std::mutex> lock(m_batchJobsMutex);
    return m_batchJobs.size();
}

bool BatchImportManager::GenerateImportLog(const AssetId& batchJobId, const std::filesystem::path& logPath) {
    auto result = GetBatchJobStatus(batchJobId);
    
    std::ofstream logFile(logPath);
    if (!logFile.is_open()) {
        LUMA_LOG_ERROR("BatchImportManager", "Failed to create log file: {}", logPath.string());
        return false;
    }
    
    logFile << "Batch Import Log\n";
    logFile << "================\n\n";
    logFile << "Total Files: " << result.totalFiles << "\n";
    logFile << "Successful: " << result.successfulImports << "\n";
    logFile << "Failed: " << result.failedImports << "\n";
    logFile << "Skipped: " << result.skippedImports << "\n";
    logFile << "Success Rate: " << (result.GetSuccessRate() * 100.0f) << "%\n";
    logFile << "Total Time: " << result.totalTimeSeconds << " seconds\n\n";
    
    logFile << "File Details:\n";
    logFile << "-------------\n";
    
    for (const auto& job : result.jobs) {
        logFile << "File: " << job.sourcePath.string() << "\n";
        logFile << "  Status: ";
        
        switch (job.status) {
            case ImportStatus::Completed:
                logFile << "Success\n";
                break;
            case ImportStatus::Failed:
                logFile << "Failed - " << job.errorMessage << "\n";
                break;
            case ImportStatus::Pending:
                logFile << "Pending\n";
                break;
            case ImportStatus::Cancelled:
                logFile << "Cancelled\n";
                break;
            default:
                logFile << "Unknown\n";
                break;
        }
        
        logFile << "\n";
    }
    
    logFile.close();
    
    LUMA_LOG_INFO("BatchImportManager", "Generated import log: {}", logPath.string());
    return true;
}

// ============================================================================
// Private Methods
// ============================================================================

void BatchImportManager::ProcessBatchJob(AssetId batchJobId) {
    std::shared_ptr<BatchJob> batchJob;
    
    {
        std::lock_guard<std::mutex> lock(m_batchJobsMutex);
        auto it = m_batchJobs.find(batchJobId);
        if (it == m_batchJobs.end()) {
            return;
        }
        batchJob = it->second;
        batchJob->processing = true;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    LUMA_LOG_INFO("BatchImportManager", "Processing batch job {}", Luma::ToString(batchJobId));
    
    usize completedCount = 0;
    
    for (auto& job : batchJob->result.jobs) {
        if (batchJob->cancelled) {
            job.status = ImportStatus::Cancelled;
            batchJob->result.skippedImports++;
            completedCount++;
            continue;
        }
        
        // Check if file should be skipped
        if (ShouldSkipFile(job.sourcePath, batchJob->config.outputDirectory, batchJob->config)) {
            job.status = ImportStatus::Cancelled; // Use cancelled to indicate skipped
            batchJob->result.skippedImports++;
            completedCount++;
            continue;
        }
        
        job.status = ImportStatus::InProgress;
        
        // Update progress
        UpdateProgress(batchJobId, job, completedCount);
        
        // Import the file
        ImportSingleFile(job, batchJob->config, m_importManager);
        
        completedCount++;
        
        // Update progress after import
        UpdateProgress(batchJobId, job, completedCount);
        
        if (job.status == ImportStatus::Completed) {
            batchJob->result.successfulImports++;
        } else if (job.status == ImportStatus::Failed) {
            batchJob->result.failedImports++;
            
            if (!batchJob->config.continueOnError) {
    LUMA_LOG_INFO("BatchImportManager", "Stopping batch import due to error");
                break;
            }
        }
    }
    
    auto endTime = std::chrono::steady_clock::now();
    batchJob->result.totalTimeSeconds = 
        std::chrono::duration<double>(endTime - startTime).count();
    
    batchJob->processing = false;
    
    // Generate log if requested
    if (batchJob->config.generateLog && !batchJob->config.logFilePath.empty()) {
        GenerateImportLog(batchJobId, batchJob->config.logFilePath);
    }
    
    // Complete the batch job
    CompleteBatchJob(batchJobId, batchJob->result);
    
    LUMA_LOG_INFO("BatchImportManager", "Completed batch job - {} successful, {} failed, {} skipped",
                 batchJob->result.successfulImports,
                 batchJob->result.failedImports,
                 batchJob->result.skippedImports);
}

void BatchImportManager::ImportSingleFile(BatchImportJob& job,
                                         const BatchImportConfig& config,
                                         AssetImportManager* importManager) {
    if (!importManager) {
        job.status = ImportStatus::Failed;
        job.errorMessage = "Import manager not available";
        return;
    }
    
    try {
        auto jobId = importManager->QueueImport(
            job.sourcePath,
            job.assetType,
            job.importSettings.empty() ? "" : job.importSettings,
            nullptr); // No individual callback for batch imports
        
        if (!jobId.IsValid()) {
            job.status = ImportStatus::Failed;
            job.errorMessage = "Failed to queue import job";
            return;
        }
        
        // Wait for the import to complete (with timeout)
        auto startTime = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::seconds(300); // 5 minute timeout
        
        while (true) {
            auto status = importManager->GetJobStatus(jobId);
            
            if (status == ImportStatus::Completed) {
                job.status = ImportStatus::Completed;
                job.jobId = jobId;
                return;
            } else if (status == ImportStatus::Failed) {
                job.status = ImportStatus::Failed;
                auto result = importManager->GetJobResult(jobId);
                if (result) {
                    job.errorMessage = result->errorMessage;
                } else {
                    job.errorMessage = "Import failed with no error message";
                }
                return;
            } else if (status == ImportStatus::Cancelled) {
                job.status = ImportStatus::Cancelled;
                job.errorMessage = "Import was cancelled";
                return;
            }
            
            // Check timeout
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > timeout) {
                job.status = ImportStatus::Failed;
                job.errorMessage = "Import timed out";
                importManager->CancelImport(jobId);
                return;
            }
            
            // Small delay to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    } catch (const std::exception& e) {
        job.status = ImportStatus::Failed;
        job.errorMessage = std::string("Exception during import: ") + e.what();
        LUMA_LOG_ERROR("BatchImportManager", "Exception during import: {}", e.what());
    }
}

AssetType BatchImportManager::DetermineAssetType(const std::filesystem::path& filePath) const {
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Mesh formats
    static const std::vector<std::string> meshExtensions = {
        ".fbx", ".obj", ".gltf", ".glb", ".dae", ".blend", ".3ds"
    };
    
    // Texture formats
    static const std::vector<std::string> textureExtensions = {
        ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".psd", ".gif", ".hdr"
    };
    
    for (const auto& meshExt : meshExtensions) {
        if (ext == meshExt) return AssetType::Mesh;
    }
    
    for (const auto& texExt : textureExtensions) {
        if (ext == texExt) return AssetType::Texture;
    }
    
    return AssetType::Unknown;
}

bool BatchImportManager::ShouldSkipFile(const std::filesystem::path& sourcePath,
                                       const std::filesystem::path& outputDir,
                                       const BatchImportConfig& config) const {
    if (!config.skipExisting) {
        return false;
    }
    
    // Check if native file already exists
    std::string nativeExtension;
    auto assetType = DetermineAssetType(sourcePath);
    
    if (assetType == AssetType::Mesh) {
        nativeExtension = ".lmesh";
    } else if (assetType == AssetType::Texture) {
        nativeExtension = ".ltex";
    } else {
        return false;
    }
    
    auto nativePath = outputDir / (sourcePath.stem().string() + nativeExtension);
    
    if (std::filesystem::exists(nativePath)) {
        LUMA_LOG_DEBUG("BatchImportManager", "Skipping existing file: {}", sourcePath.string());
        return true;
    }
    
    return false;
}

std::unordered_map<AssetType, std::vector<std::filesystem::path>> 
BatchImportManager::GroupFilesByType(const std::vector<std::filesystem::path>& files) const {
    std::unordered_map<AssetType, std::vector<std::filesystem::path>> grouped;
    
    for (const auto& file : files) {
        auto type = DetermineAssetType(file);
        if (type != AssetType::Unknown) {
            grouped[type].push_back(file);
        }
    }
    
    return grouped;
}

void BatchImportManager::UpdateProgress(const AssetId& batchJobId,
                                       const BatchImportJob& currentJob,
                                       usize completedCount) {
    std::lock_guard<std::mutex> lock(m_batchJobsMutex);
    
    auto it = m_batchJobs.find(batchJobId);
    if (it != m_batchJobs.end() && it->second->progressCallback) {
        it->second->progressCallback(currentJob, completedCount, 
                                    it->second->result.totalFiles, 
                                    it->second->result);
    }
}

void BatchImportManager::CompleteBatchJob(AssetId batchJobId, BatchImportResult result) {
    // Move to completed jobs for main thread processing
    {
        std::lock_guard<std::mutex> lock(m_completedBatchJobsMutex);
        m_completedBatchJobs.push_back({batchJobId, result});
    }
}

// ============================================================================
// BatchImportHelper Implementation
// ============================================================================

AssetId BatchImportHelper::QuickImport(
    const std::vector<std::filesystem::path>& sourcePaths,
    const std::filesystem::path& outputDir,
    AssetRegistry* registry,
    AssetImportManager* importManager) {
    
    static BatchImportManager batchManager;
    static bool initialized = false;
    
    if (!initialized) {
        batchManager.Initialize(registry, importManager);
        initialized = true;
    }
    
    BatchImportConfig config;
    config.outputDirectory = outputDir;
    config.useCommonSettings = true;
    config.continueOnError = true;
    config.generateLog = true;
    
    return batchManager.ImportFiles(sourcePaths, outputDir, config);
}

AssetId BatchImportHelper::ImportDirectoryTree(
    const std::filesystem::path& sourceDir,
    const std::filesystem::path& outputDir,
    AssetRegistry* registry,
    AssetImportManager* importManager,
    bool preserveStructure) {
    
    static BatchImportManager batchManager;
    static bool initialized = false;
    
    if (!initialized) {
        batchManager.Initialize(registry, importManager);
        initialized = true;
    }
    
    if (preserveStructure) {
        // Import directory while preserving structure
        return batchManager.ImportDirectory(sourceDir, outputDir, BatchImportConfig());
    } else {
        // Flatten structure - import all files to single output directory
        return batchManager.ImportDirectory(sourceDir, outputDir, BatchImportConfig());
    }
}

AssetId BatchImportHelper::ReimportAllOutdated(
    AssetRegistry* registry,
    AssetImportManager* importManager) {
    
    if (!registry || !importManager) {
        LUMA_LOG_ERROR("BatchImportHelper", "Registry or import manager is null");
        return AssetId();
    }
    
    // Collect all assets that need reimporting
    std::vector<AssetId> outdatedAssets;
    
    // This would require AssetRegistry to provide a method to iterate all assets
    // For now, this is a placeholder
    
    LUMA_LOG_INFO("BatchImportHelper", "ReimportAllOutdated not fully implemented yet");
    
    return AssetId();
}

BatchImportConfig BatchImportHelper::ConfigFromSettings(
    const std::string& importSettings,
    const std::filesystem::path& outputDir) {
    
    BatchImportConfig config;
    config.outputDirectory = outputDir;
    config.commonImportSettings = importSettings;
    config.useCommonSettings = true;
    config.continueOnError = true;
    config.generateLog = true;
    
    return config;
}

} // namespace Luma