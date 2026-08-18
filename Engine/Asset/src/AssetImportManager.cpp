#include "Luma/Asset/AssetImportManager.h"
#include "Luma/Asset/TextureImporter.h"
#include "Luma/Asset/ThumbnailRenderer.h"
#include "Luma/Asset/TextureThumbnailRenderer.h"
#include "Luma/Asset/MeshThumbnailRenderer.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>
#include <filesystem>

#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Asset/AssetMetadata.h"
#include "Luma/Core/Log.h"
#include "Luma/Serialization/Json.h"
#include "Luma/Serialization/SerialValue.h"

namespace Luma {

// Singleton instance
static std::unique_ptr<AssetImportManager> g_globalImportManager;

AssetImportManager& Global() {
    if (!g_globalImportManager) {
        g_globalImportManager = std::make_unique<AssetImportManager>();
    }
    return *g_globalImportManager;
}

std::string GlobalImportSettings::ToJson() const {
    SerialValue obj = SerialValue::MakeObject();
    
    SerialValue meshObj = SerialValue::MakeObject();
    meshObj["generateNormals"] = mesh.generateNormals;
    meshObj["generateTangents"] = mesh.generateTangents;
    meshObj["flipUVs"] = mesh.flipUVs;
    meshObj["optimizeMesh"] = mesh.optimizeMesh;
    meshObj["optimizeVertexCache"] = mesh.optimizeVertexCache;
    meshObj["optimizeVertexFetch"] = mesh.optimizeVertexFetch;
    meshObj["scale"] = mesh.scale;
    meshObj["importSkeleton"] = mesh.importSkeleton;
    meshObj["importAnimations"] = mesh.importAnimations;
    obj["mesh"] = std::move(meshObj);
    
    SerialValue textureObj = SerialValue::MakeObject();
    textureObj["generateMipmaps"] = texture.generateMipmaps;
    textureObj["sRGB"] = texture.sRGB;
    textureObj["compress"] = texture.compress;
    textureObj["normalMap"] = texture.normalMap;
    textureObj["maxTextureSize"] = texture.maxTextureSize;
    textureObj["desiredSize"] = texture.desiredSize;
    textureObj["scale"] = texture.scale;
    textureObj["filterType"] = texture.filterType;
    textureObj["preserveAlpha"] = texture.preserveAlpha;
    textureObj["flipY"] = texture.flipY;
    textureObj["premultiplyAlpha"] = texture.premultiplyAlpha;
    textureObj["alphaThreshold"] = texture.alphaThreshold;
    textureObj["packChannels"] = texture.packChannels;
    textureObj["redChannel"] = texture.redChannel;
    textureObj["greenChannel"] = texture.greenChannel;
    textureObj["blueChannel"] = texture.blueChannel;
    textureObj["alphaChannel"] = texture.alphaChannel;
    obj["texture"] = std::move(textureObj);
    
    return WriteJson(obj, true);
}

GlobalImportSettings GlobalImportSettings::FromJson(const std::string& json) {
    GlobalImportSettings settings = GetDefaults();
    
    std::string error;
    auto value = ParseJson(json, &error);
    if (!value || !value->IsObject()) {
        LUMA_LOG_WARN("AssetImportManager", "Failed to parse global import settings: {}", error);
        return settings;
    }
    
    const SerialValue& obj = *value;
    if (auto v = obj.Find("mesh"); v && v->IsObject()) {
        const SerialValue& meshObj = *v;
        if (auto mv = meshObj.Find("generateNormals"); mv && mv->IsBool())
            settings.mesh.generateNormals = mv->AsBool();
        if (auto mv = meshObj.Find("generateTangents"); mv && mv->IsBool())
            settings.mesh.generateTangents = mv->AsBool();
        if (auto mv = meshObj.Find("flipUVs"); mv && mv->IsBool())
            settings.mesh.flipUVs = mv->AsBool();
        if (auto mv = meshObj.Find("optimizeMesh"); mv && mv->IsBool())
            settings.mesh.optimizeMesh = mv->AsBool();
        if (auto mv = meshObj.Find("optimizeVertexCache"); mv && mv->IsBool())
            settings.mesh.optimizeVertexCache = mv->AsBool();
        if (auto mv = meshObj.Find("optimizeVertexFetch"); mv && mv->IsBool())
            settings.mesh.optimizeVertexFetch = mv->AsBool();
        if (auto mv = meshObj.Find("scale"); mv && mv->IsNumber())
            settings.mesh.scale = static_cast<f32>(mv->AsFloat());
        if (auto mv = meshObj.Find("importSkeleton"); mv && mv->IsBool())
            settings.mesh.importSkeleton = mv->AsBool();
        if (auto mv = meshObj.Find("importAnimations"); mv && mv->IsBool())
            settings.mesh.importAnimations = mv->AsBool();
    }
    
    if (auto v = obj.Find("texture"); v && v->IsObject()) {
        const SerialValue& textureObj = *v;
        if (auto tv = textureObj.Find("generateMipmaps"); tv && tv->IsBool())
            settings.texture.generateMipmaps = tv->AsBool();
        if (auto tv = textureObj.Find("sRGB"); tv && tv->IsBool())
            settings.texture.sRGB = tv->AsBool();
        if (auto tv = textureObj.Find("compress"); tv && tv->IsBool())
            settings.texture.compress = tv->AsBool();
        if (auto tv = textureObj.Find("normalMap"); tv && tv->IsBool())
            settings.texture.normalMap = tv->AsBool();
        if (auto tv = textureObj.Find("maxTextureSize"); tv && tv->IsNumber())
            settings.texture.maxTextureSize = static_cast<i32>(tv->AsInt());
        if (auto tv = textureObj.Find("desiredSize"); tv && tv->IsNumber())
            settings.texture.desiredSize = static_cast<i32>(tv->AsInt());
        if (auto tv = textureObj.Find("scale"); tv && tv->IsNumber())
            settings.texture.scale = static_cast<f32>(tv->AsFloat());
        if (auto tv = textureObj.Find("filterType"); tv && tv->IsNumber())
            settings.texture.filterType = static_cast<i32>(tv->AsInt());
        if (auto tv = textureObj.Find("preserveAlpha"); tv && tv->IsBool())
            settings.texture.preserveAlpha = tv->AsBool();
        if (auto tv = textureObj.Find("flipY"); tv && tv->IsBool())
            settings.texture.flipY = tv->AsBool();
        if (auto tv = textureObj.Find("premultiplyAlpha"); tv && tv->IsBool())
            settings.texture.premultiplyAlpha = tv->AsBool();
        if (auto tv = textureObj.Find("alphaThreshold"); tv && tv->IsNumber())
            settings.texture.alphaThreshold = static_cast<i32>(tv->AsInt());
        if (auto tv = textureObj.Find("packChannels"); tv && tv->IsBool())
            settings.texture.packChannels = tv->AsBool();
        if (auto tv = textureObj.Find("redChannel"); tv && tv->IsNumber())
            settings.texture.redChannel = static_cast<i32>(tv->AsInt());
        if (auto tv = textureObj.Find("greenChannel"); tv && tv->IsNumber())
            settings.texture.greenChannel = static_cast<i32>(tv->AsInt());
        if (auto tv = textureObj.Find("blueChannel"); tv && tv->IsNumber())
            settings.texture.blueChannel = static_cast<i32>(tv->AsInt());
        if (auto tv = textureObj.Find("alphaChannel"); tv && tv->IsNumber())
            settings.texture.alphaChannel = static_cast<i32>(tv->AsInt());
    }
    
    return settings;
}

GlobalImportSettings GlobalImportSettings::GetDefaults() {
    return GlobalImportSettings{};  // Default-initialized
}

AssetImportManager::AssetImportManager() {
    m_globalSettings = GlobalImportSettings::GetDefaults();
}

AssetImportManager::~AssetImportManager() {
    Shutdown();
}

void AssetImportManager::Initialize(AssetRegistry* registry) {
    m_registry = registry;
    
    // Start worker threads
    for (usize i = 0; i < kWorkerThreadCount; ++i) {
        m_workerThreads.emplace_back(&AssetImportManager::WorkerThread, this);
    }
    
    LUMA_LOG_INFO("AssetImportManager", "Initialized with {} worker threads", kWorkerThreadCount);
}

void AssetImportManager::Shutdown() {
    if (m_shutdown) return;
    
    m_shutdown = true;
    m_jobQueueCV.notify_all();
    
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    m_workerThreads.clear();
    LUMA_LOG_INFO("AssetImportManager", "Shutdown complete");
}

void AssetImportManager::RegisterImporter(std::unique_ptr<IAssetImporter> importer) {
    if (!importer) return;
    
    AssetType type = importer->GetAssetType();
    
    // Check if we already have an importer for this type
    auto it = std::find_if(m_importers.begin(), m_importers.end(),
        [type](const auto& imp) { return imp->GetAssetType() == type; });
    
    if (it != m_importers.end()) {
        LUMA_LOG_WARN("AssetImportManager", "Replacing existing importer for type {}", 
                      AssetTypeName(type));
        *it = std::move(importer);
    } else {
        m_importers.push_back(std::move(importer));
        LUMA_LOG_INFO("AssetImportManager", "Registered importer for type {}", 
                      AssetTypeName(type));
    }
}

AssetId AssetImportManager::QueueImport(const std::filesystem::path& sourcePath,
                                        AssetType type,
                                        const std::string& importSettings,
                                        std::function<void(const ImportResult&)> onComplete) {
    if (!m_registry) {
        LUMA_LOG_ERROR("AssetImportManager", "Cannot queue import: no registry set");
        return AssetId{};
    }
    
    // Generate job ID
    AssetId jobId = MakeAssetIdFromKey(sourcePath.string() + "|" + 
                                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    
    ImportJob job;
    job.jobId = jobId;
    job.sourcePath = sourcePath;
    job.assetType = type;
    job.isReimport = false;
    job.importSettings = importSettings.empty() ? 
        GetImporter(type)->GetDefaultSettings() : importSettings;
    job.onComplete = std::move(onComplete);
    
    // Set job status to pending
    {
        std::lock_guard<std::mutex> lock(m_jobResultsMutex);
        ImportResult result;
        result.status = ImportStatus::Pending;
        m_jobResults[jobId] = std::move(result);
    }
    
    // Add to queue
    {
        std::lock_guard<std::mutex> lock(m_jobQueueMutex);
        m_jobQueue.push(std::move(job));
    }
    
    m_jobQueueCV.notify_one();
    
    LUMA_LOG_INFO("AssetImportManager", "Queued import job for: {}", sourcePath.string());
    return jobId;
}

AssetId AssetImportManager::QueueReimport(const AssetId& assetId,
                                         std::function<void(const ImportResult&)> onComplete) {
    if (!m_registry) {
        LUMA_LOG_ERROR("AssetImportManager", "Cannot queue reimport: no registry set");
        return AssetId{};
    }
    
    // Look up asset metadata
    const AssetData* data = m_registry->Lookup(assetId);
    if (!data) {
        LUMA_LOG_ERROR("AssetImportManager", "Cannot queue reimport: asset not found");
        return AssetId{};
    }
    
    // Read metadata
    auto metaPath = AssetMetadataIO::MetaPathForSource(data->packagePath);
    auto meta = AssetMetadataIO::Read(metaPath);
    if (!meta) {
        LUMA_LOG_WARN("AssetImportManager", "No metadata found for reimport: {}", 
                      data->packagePath.string());
        // Fall back to treating as new import
        return QueueImport(data->packagePath, data->type, "", std::move(onComplete));
    }
    
    // Generate job ID
    AssetId jobId = MakeAssetIdFromKey(meta->sourcePath.string() + "|reimport|" + 
                                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    
    ImportJob job;
    job.jobId = jobId;
    job.sourcePath = meta->sourcePath;
    job.assetType = meta->type;
    job.isReimport = true;
    job.importSettings = meta->importSettings.empty() ? 
        GetImporter(meta->type)->GetDefaultSettings() : meta->importSettings;
    job.onComplete = std::move(onComplete);
    
    // Set job status to pending
    {
        std::lock_guard<std::mutex> lock(m_jobResultsMutex);
        ImportResult result;
        result.status = ImportStatus::Pending;
        m_jobResults[jobId] = std::move(result);
    }
    
    // Add to queue
    {
        std::lock_guard<std::mutex> lock(m_jobQueueMutex);
        m_jobQueue.push(std::move(job));
    }
    
    m_jobQueueCV.notify_one();
    
    LUMA_LOG_INFO("AssetImportManager", "Queued reimport job for: {}", meta->sourcePath.string());
    return jobId;
}

void AssetImportManager::CancelImport(const AssetId& jobId) {
    std::lock_guard<std::mutex> lock(m_jobResultsMutex);
    auto it = m_jobResults.find(jobId);
    if (it != m_jobResults.end() && it->second.status == ImportStatus::Pending) {
        it->second.status = ImportStatus::Cancelled;
        LUMA_LOG_INFO("AssetImportManager", "Cancelled import job");
    }
}

ImportStatus AssetImportManager::GetJobStatus(const AssetId& jobId) const {
    std::lock_guard<std::mutex> lock(m_jobResultsMutex);
    auto it = m_jobResults.find(jobId);
    if (it != m_jobResults.end()) {
        return it->second.status;
    }
    return ImportStatus::Failed;  // Unknown job = failed
}

std::optional<ImportResult> AssetImportManager::GetJobResult(const AssetId& jobId) const {
    std::lock_guard<std::mutex> lock(m_jobResultsMutex);
    auto it = m_jobResults.find(jobId);
    if (it != m_jobResults.end()) {
        return it->second;
    }
    return std::nullopt;
}

void AssetImportManager::ProcessCompletedJobs() {
    std::vector<std::pair<ImportJob, ImportResult>> completed;
    
    {
        std::lock_guard<std::mutex> lock(m_completedJobsMutex);
        completed = std::move(m_completedJobs);
        m_completedJobs.clear();
    }
    
    for (auto& [job, result] : completed) {
        // Update job results
        {
            std::lock_guard<std::mutex> lock(m_jobResultsMutex);
            m_jobResults[job.jobId] = result;
        }
        
        // Call completion callback
        if (job.onComplete) {
            job.onComplete(result);
        }
        
        // Update registry if successful
        if (result.status == ImportStatus::Completed && m_registry) {
            m_registry->RefreshPath(job.sourcePath);
            if (!result.nativePath.empty()) {
                m_registry->RefreshPath(result.nativePath);
            }
            
            // Generate thumbnail after successful import. Drop the cache
            // entry first so a stale on-disk thumbnail from a previous
            // import doesn't get returned to the Content Browser before
            // the new render finishes. The thumbnail is written to the
            // project-scoped cache (<project>/Intermediate/Thumbnails) —
            // never into the content folder beside the source asset.
            auto& thumbnailMgr = ThumbnailManager::Get();
            thumbnailMgr.Invalidate(result.assetId);

            ThumbnailSettings thumbSettings;
            thumbSettings.width = 128;
            thumbSettings.height = 128;
            thumbSettings.autoRegenerate = true;

            thumbnailMgr.GenerateThumbnail(
                result.assetId, result.nativePath,
                thumbnailMgr.GetCacheDirectory() /
                    (ToString(result.assetId) + "_thumb.png"),
                thumbSettings);

            LUMA_LOG_INFO("AssetImportManager", "Generated thumbnail for imported asset: {}",
                          ToString(result.assetId));
        }
    }
}

bool AssetImportManager::SaveGlobalSettings(const std::filesystem::path& path) {
    try {
        std::ofstream file(path.string());
        if (!file) {
            LUMA_LOG_ERROR("AssetImportManager", "Failed to open file for writing: {}", path.string());
            return false;
        }
        
        std::string json = m_globalSettings.ToJson();
        file << json;
        file.close();
        
        LUMA_LOG_INFO("AssetImportManager", "Saved global import settings to {}", path.string());
        return true;
    } catch (const std::exception& e) {
        LUMA_LOG_ERROR("AssetImportManager", "Failed to save global settings: {}", e.what());
        return false;
    }
}

bool AssetImportManager::LoadGlobalSettings(const std::filesystem::path& path) {
    try {
        std::ifstream file(path.string());
        if (!file) {
            LUMA_LOG_WARN("AssetImportManager", "Failed to open file for reading: {}", path.string());
            return false;
        }
        
        std::string json((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
        file.close();
        
        m_globalSettings = GlobalImportSettings::FromJson(json);
        LUMA_LOG_INFO("AssetImportManager", "Loaded global import settings from {}", path.string());
        return true;
    } catch (const std::exception& e) {
        LUMA_LOG_ERROR("AssetImportManager", "Failed to load global settings: {}", e.what());
        return false;
    }
}

bool AssetImportManager::NeedsReimport(const AssetMetadata& meta) const {
    // Check if source file still exists
    std::error_code ec;
    if (!std::filesystem::exists(meta.sourcePath, ec)) {
        return false;  // Source gone, can't reimport
    }
    
    // Get current file stats
    auto currentMtime = std::chrono::duration_cast<std::chrono::seconds>(
        std::filesystem::last_write_time(meta.sourcePath, ec).time_since_epoch()).count();
    auto currentHash = AssetMetadataIO::ComputeFileHash(meta.sourcePath);
    
    return meta.SourceChanged(currentMtime, currentHash);
}

IAssetImporter* AssetImportManager::GetImporter(AssetType type) const {
    auto it = std::find_if(m_importers.begin(), m_importers.end(),
        [type](const auto& imp) { return imp->GetAssetType() == type; });
    return (it != m_importers.end()) ? it->get() : nullptr;
}

usize AssetImportManager::GetActiveJobCount() const {
    std::lock_guard<std::mutex> lock(m_jobResultsMutex);
    usize count = 0;
    for (const auto& [id, result] : m_jobResults) {
        if (result.status == ImportStatus::Pending || result.status == ImportStatus::InProgress) {
            ++count;
        }
    }
    return count;
}

void AssetImportManager::WorkerThread() {
    LUMA_LOG_INFO("AssetImportManager", "Worker thread started");
    
    while (!m_shutdown) {
        ImportJob job;
        
        // Wait for job
        {
            std::unique_lock<std::mutex> lock(m_jobQueueMutex);
            m_jobQueueCV.wait(lock, [this] {
                return m_shutdown || !m_jobQueue.empty();
            });
            
            if (m_shutdown) break;
            
            if (!m_jobQueue.empty()) {
                job = std::move(m_jobQueue.front());
                m_jobQueue.pop();
            } else {
                continue;
            }
        }
        
        // Check if cancelled
        {
            std::lock_guard<std::mutex> lock(m_jobResultsMutex);
            auto it = m_jobResults.find(job.jobId);
            if (it != m_jobResults.end() && it->second.status == ImportStatus::Cancelled) {
                continue;
            }
        }
        
        // Process job
        ProcessJob(job);
    }
    
    LUMA_LOG_INFO("AssetImportManager", "Worker thread exiting");
}

void AssetImportManager::ProcessJob(ImportJob& job) {
    // Update status to in progress
    {
        std::lock_guard<std::mutex> lock(m_jobResultsMutex);
        auto it = m_jobResults.find(job.jobId);
        if (it != m_jobResults.end()) {
            it->second.status = ImportStatus::InProgress;
        }
    }
    
    ImportResult result;
    result.status = ImportStatus::Failed;
    result.assetId = job.jobId;
    
    try {
        // Get importer
        IAssetImporter* importer = GetImporter(job.assetType);
        if (!importer) {
            result.errorMessage = "No importer registered for asset type: " + 
                                 std::string(AssetTypeName(job.assetType));
            CompleteJob(std::move(job), result);
            return;
        }
        
        // Determine output directory (same as source)
        auto outputDir = job.sourcePath.parent_path();
        
        // Run import
        result = importer->Import(job.sourcePath, job.importSettings, outputDir);
        
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Import exception: ") + e.what();
        result.status = ImportStatus::Failed;
        LUMA_LOG_ERROR("AssetImportManager", "Import failed for {}: {}", 
                       job.sourcePath.string(), result.errorMessage);
    }
    
    CompleteJob(std::move(job), result);
}

void AssetImportManager::CompleteJob(ImportJob&& job, ImportResult result) {
    result.importTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Log before moving
    LUMA_LOG_INFO("AssetImportManager", "Import job completed: {} (status: {})", 
                  job.sourcePath.string(),
                  static_cast<int>(result.status));
    
    // Move to completed jobs queue for main thread processing
    {
        std::lock_guard<std::mutex> lock(m_completedJobsMutex);
        m_completedJobs.emplace_back(std::move(job), std::move(result));
    }
}

}  // namespace Luma
