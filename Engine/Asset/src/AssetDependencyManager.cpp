#include "Luma/Asset/AssetDependencyManager.h"

#include <algorithm>
#include <sstream>

#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Core/Log.h"

namespace Luma {

// ============================================================================
// AssetDependencyManager Implementation
// ============================================================================

AssetDependencyManager::AssetDependencyManager()
    : m_registry(nullptr)
    , m_graphMutex() {
    LUMA_LOG_INFO("AssetDependencyManager", "Initialized");
}

AssetDependencyManager::~AssetDependencyManager() {
    LUMA_LOG_INFO("AssetDependencyManager", "Shutdown");
}

void AssetDependencyManager::Initialize(AssetRegistry* registry) {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    m_registry = registry;
    LUMA_LOG_INFO("AssetDependencyManager", "Initialized with asset registry");
}

void AssetDependencyManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    m_graph.outgoing.clear();
    m_graph.incoming.clear();
    m_graph.dependencyDetails.clear();
    m_registry = nullptr;
    LUMA_LOG_INFO("AssetDependencyManager", "Shutdown completed");
}

void AssetDependencyManager::AddDependency(const AssetId& dependentAsset,
                                          const AssetId& dependencyAsset,
                                          const std::string& dependencyType,
                                          bool isHardDependency) {
    if (dependentAsset == dependencyAsset) {
        LUMA_LOG_WARN("AssetDependencyManager", "Cannot add self-dependency for asset");
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_graphMutex);
    
    m_graph.outgoing[dependentAsset].insert(dependencyAsset);
    m_graph.incoming[dependencyAsset].insert(dependentAsset);
    
    AssetDependency info{};
    info.dependentAsset = dependentAsset;
    info.dependencyAsset = dependencyAsset;
    info.dependencyType = dependencyType;
    info.isHardDependency = isHardDependency;
    info.isRuntimeDependency = true;
    
    m_graph.dependencyDetails[dependentAsset] = info;
    
    LUMA_LOG_DEBUG("AssetDependencyManager", "Added dependency: {} -> {} ({})",
                   Luma::ToString(dependentAsset), Luma::ToString(dependencyAsset), dependencyType);
}

void AssetDependencyManager::RemoveDependency(const AssetId& dependentAsset,
                                             const AssetId& dependencyAsset) {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    
    auto outIt = m_graph.outgoing.find(dependentAsset);
    if (outIt != m_graph.outgoing.end()) {
        outIt->second.erase(dependencyAsset);
        if (outIt->second.empty()) {
            m_graph.outgoing.erase(outIt);
        }
    }
    
    auto inIt = m_graph.incoming.find(dependencyAsset);
    if (inIt != m_graph.incoming.end()) {
        inIt->second.erase(dependentAsset);
        if (inIt->second.empty()) {
            m_graph.incoming.erase(inIt);
        }
    }
    
    m_graph.dependencyDetails.erase(dependentAsset);
}

void AssetDependencyManager::RemoveAllDependencies(const AssetId& assetId) {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    
    auto outIt = m_graph.outgoing.find(assetId);
    if (outIt != m_graph.outgoing.end()) {
        for (const auto& dep : outIt->second) {
            auto inIt = m_graph.incoming.find(dep);
            if (inIt != m_graph.incoming.end()) {
                inIt->second.erase(assetId);
                if (inIt->second.empty()) {
                    m_graph.incoming.erase(inIt);
                }
            }
        }
        m_graph.outgoing.erase(outIt);
    }
    
    auto inIt = m_graph.incoming.find(assetId);
    if (inIt != m_graph.incoming.end()) {
        for (const auto& dependent : inIt->second) {
            auto oIt = m_graph.outgoing.find(dependent);
            if (oIt != m_graph.outgoing.end()) {
                oIt->second.erase(assetId);
                if (oIt->second.empty()) {
                    m_graph.outgoing.erase(oIt);
                }
            }
        }
        m_graph.incoming.erase(inIt);
    }
    
    m_graph.dependencyDetails.erase(assetId);
}

std::vector<AssetId> AssetDependencyManager::GetDependents(const AssetId& assetId) const {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    
    auto it = m_graph.incoming.find(assetId);
    if (it != m_graph.incoming.end()) {
        return std::vector<AssetId>(it->second.begin(), it->second.end());
    }
    
    return {};
}

std::vector<AssetId> AssetDependencyManager::GetDependencies(const AssetId& assetId) const {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    
    auto it = m_graph.outgoing.find(assetId);
    if (it != m_graph.outgoing.end()) {
        return std::vector<AssetId>(it->second.begin(), it->second.end());
    }
    
    return {};
}

std::vector<AssetDependency> AssetDependencyManager::GetDependencyInfo(const AssetId& assetId) const {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    
    std::vector<AssetDependency> result;
    auto it = m_graph.outgoing.find(assetId);
    if (it != m_graph.outgoing.end()) {
        for (const auto& depAsset : it->second) {
            auto detailIt = m_graph.dependencyDetails.find(assetId);
            if (detailIt != m_graph.dependencyDetails.end() && detailIt->second.dependencyAsset == depAsset) {
                result.push_back(detailIt->second);
            } else {
                AssetDependency info{};
                info.dependentAsset = assetId;
                info.dependencyAsset = depAsset;
                info.dependencyType = "generic";
                info.isHardDependency = true;
                info.isRuntimeDependency = true;
                result.push_back(info);
            }
        }
    }
    return result;
}

bool AssetDependencyManager::HasDependencies(const AssetId& assetId) const {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    auto it = m_graph.outgoing.find(assetId);
    return it != m_graph.outgoing.end() && !it->second.empty();
}

bool AssetDependencyManager::HasDependents(const AssetId& assetId) const {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    auto it = m_graph.incoming.find(assetId);
    return it != m_graph.incoming.end() && !it->second.empty();
}

bool AssetDependencyManager::DependsOn(const AssetId& assetA, const AssetId& assetB) const {
    auto transitive = GetTransitiveDependencies(assetA);
    return std::find(transitive.begin(), transitive.end(), assetB) != transitive.end();
}

std::vector<AssetId> AssetDependencyManager::GetTransitiveDependencies(const AssetId& assetId) const {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    std::unordered_set<AssetId> visited;
    std::vector<AssetId> result;
    FindTransitiveDependencies(assetId, visited, result);
    return result;
}

void AssetDependencyManager::FindTransitiveDependencies(const AssetId& assetId,
                                                         std::unordered_set<AssetId>& visited,
                                                         std::vector<AssetId>& result) const {
    auto it = m_graph.outgoing.find(assetId);
    if (it == m_graph.outgoing.end()) return;
    
    for (const auto& dep : it->second) {
        if (visited.insert(dep).second) {
            result.push_back(dep);
            FindTransitiveDependencies(dep, visited, result);
        }
    }
}

std::vector<AssetId> AssetDependencyManager::GetTransitiveDependents(const AssetId& assetId) const {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    std::unordered_set<AssetId> visited;
    std::vector<AssetId> result;
    FindTransitiveDependents(assetId, visited, result);
    return result;
}

void AssetDependencyManager::FindTransitiveDependents(const AssetId& assetId,
                                                       std::unordered_set<AssetId>& visited,
                                                       std::vector<AssetId>& result) const {
    auto it = m_graph.incoming.find(assetId);
    if (it == m_graph.incoming.end()) return;
    
    for (const auto& dependent : it->second) {
        if (visited.insert(dependent).second) {
            result.push_back(dependent);
            FindTransitiveDependents(dependent, visited, result);
        }
    }
}

std::vector<AssetId> AssetDependencyManager::NotifyAssetChanged(const AssetId& changedAsset,
                                                                 const std::string& changeType) {
    auto affected = GetTransitiveDependents(changedAsset);
    
    DependencyChangeNotification notification{};
    notification.changedAsset = changedAsset;
    notification.affectedAssets = affected;
    notification.changeType = changeType;
    
    std::lock_guard<std::mutex> lock(m_callbacksMutex);
    for (const auto& callback : m_changeCallbacks) {
        if (callback) {
            callback(notification);
        }
    }
    
    return affected;
}

void AssetDependencyManager::RegisterChangeCallback(std::function<void(const DependencyChangeNotification&)> callback) {
    std::lock_guard<std::mutex> lock(m_callbacksMutex);
    m_changeCallbacks.push_back(callback);
}

void AssetDependencyManager::UnregisterChangeCallback(std::function<void(const DependencyChangeNotification&)> /*callback*/) {
    // Unregister callback
}

void AssetDependencyManager::AnalyzeAssetDependencies(const AssetId& assetId) {
    if (!m_registry) return;
    LUMA_LOG_DEBUG("AssetDependencyManager", "Analyzing dependencies for asset {}", Luma::ToString(assetId));
}

void AssetDependencyManager::RebuildDependencyGraph() {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    m_graph.outgoing.clear();
    m_graph.incoming.clear();
    m_graph.dependencyDetails.clear();
    LUMA_LOG_INFO("AssetDependencyManager", "Rebuilt dependency graph");
}

std::string AssetDependencyManager::ExportDependencyGraphToDOT() const {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    std::stringstream ss;
    ss << "digraph AssetDependencies {\n";
    ss << "  rankdir=LR;\n";
    ss << "  node [shape=box];\n";
    for (const auto& [assetId, deps] : m_graph.outgoing) {
        for (const auto& dep : deps) {
            ss << "  \"" << Luma::ToString(assetId) << "\" -> \"" << Luma::ToString(dep) << "\";\n";
        }
    }
    ss << "}\n";
    return ss.str();
}

bool AssetDependencyManager::DetectCycle(const AssetId& assetId,
                                          std::unordered_set<AssetId>& visited,
                                          std::unordered_set<AssetId>& recursionStack,
                                          std::vector<AssetId>& cycle) const {
    visited.insert(assetId);
    recursionStack.insert(assetId);
    cycle.push_back(assetId);
    
    auto it = m_graph.outgoing.find(assetId);
    if (it != m_graph.outgoing.end()) {
        for (const auto& dep : it->second) {
            if (recursionStack.count(dep) > 0) {
                cycle.push_back(dep);
                return true;
            }
            if (visited.count(dep) == 0) {
                if (DetectCycle(dep, visited, recursionStack, cycle)) {
                    return true;
                }
            }
        }
    }
    
    recursionStack.erase(assetId);
    cycle.pop_back();
    return false;
}

std::vector<std::vector<AssetId>> AssetDependencyManager::FindCircularDependencies() const {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    std::vector<std::vector<AssetId>> cycles;
    std::unordered_set<AssetId> visited;
    
    for (const auto& [assetId, _] : m_graph.outgoing) {
        if (visited.count(assetId) == 0) {
            std::unordered_set<AssetId> recursionStack;
            std::vector<AssetId> cycle;
            if (DetectCycle(assetId, visited, recursionStack, cycle)) {
                cycles.push_back(cycle);
            }
        }
    }
    
    return cycles;
}

AssetDependencyManager::DependencyStats AssetDependencyManager::GetStatistics() const {
    std::lock_guard<std::mutex> lock(m_graphMutex);
    DependencyStats stats{};
    std::unordered_set<AssetId> allAssets;
    usize totalDeps = 0;
    usize maxDeps = 0;
    AssetId maxAsset{};
    
    for (const auto& [assetId, deps] : m_graph.outgoing) {
        allAssets.insert(assetId);
        if (!deps.empty()) {
            stats.assetsWithDependencies++;
            totalDeps += deps.size();
            if (deps.size() > maxDeps) {
                maxDeps = deps.size();
                maxAsset = assetId;
            }
        }
        for (const auto& dep : deps) {
            allAssets.insert(dep);
        }
    }
    
    for (const auto& [assetId, dependents] : m_graph.incoming) {
        allAssets.insert(assetId);
        if (!dependents.empty()) {
            stats.assetsWithDependents++;
        }
    }
    
    stats.totalAssets = allAssets.size();
    stats.totalDependencies = totalDeps;
    stats.maxDependencies = maxDeps;
    stats.assetWithMostDependencies = maxAsset;
    if (stats.assetsWithDependencies > 0) {
        stats.averageDependenciesPerAsset = totalDeps / stats.assetsWithDependencies;
    }
    
    return stats;
}

// ============================================================================
// DependencyHelper Implementation
// ============================================================================

std::vector<AssetId> DependencyHelper::GetAffectedAssetsOnDelete(const AssetId& assetId,
                                                                  AssetDependencyManager* depManager) {
    if (!depManager) return {};
    return depManager->GetTransitiveDependents(assetId);
}

bool DependencyHelper::IsSafeToDelete(const AssetId& assetId, AssetDependencyManager* depManager) {
    if (!depManager) return true;
    return !depManager->HasDependents(assetId);
}

DependencyHelper::DeletionImpact DependencyHelper::AnalyzeDeletionImpact(const AssetId& assetId, 
                                                                          AssetDependencyManager* depManager) {
    DeletionImpact impact{};
    if (!depManager) return impact;
    
    auto direct = depManager->GetDependents(assetId);
    auto transitive = depManager->GetTransitiveDependents(assetId);
    
    impact.directDependents = direct.size();
    impact.transitiveDependents = transitive.size();
    impact.affectedAssets = transitive;
    
    for (const auto& depAsset : direct) {
        auto infoList = depManager->GetDependencyInfo(depAsset);
        for (const auto& info : infoList) {
            if (info.dependencyAsset == assetId && info.isHardDependency) {
                impact.hasHardDependencies = true;
                break;
            }
        }
    }
    
    return impact;
}

std::vector<AssetId> DependencyHelper::FindReferencingAssets(const AssetId& assetId, AssetRegistry* /*registry*/) {
    (void)assetId;
    return {};
}

// ============================================================================
// DependencyValidator Implementation
// ============================================================================

DependencyValidator::ValidationResult DependencyValidator::Validate(AssetDependencyManager* depManager,
                                                                      AssetRegistry* /*registry*/) {
    ValidationResult result{};
    if (!depManager) return result;
    
    result.circularDependencies = depManager->FindCircularDependencies();
    if (!result.circularDependencies.empty()) {
        result.isValid = false;
        result.errors.push_back("Found circular dependencies in dependency graph");
    }
    
    return result;
}

DependencyValidator::ValidationResult DependencyValidator::ValidateAsset(const AssetId& assetId,
                                                                           AssetDependencyManager* depManager,
                                                                           AssetRegistry* registry) {
    ValidationResult result{};
    if (!depManager) return result;
    
    auto dependencies = depManager->GetDependencies(assetId);
    if (registry) {
        for (const auto& dep : dependencies) {
            if (!registry->Lookup(dep)) {
                result.isValid = false;
                result.errors.push_back("Missing dependency for asset: " + Luma::ToString(dep));
                result.missingDependencies.push_back(dep);
            }
        }
    }
    
    return result;
}

} // namespace Luma