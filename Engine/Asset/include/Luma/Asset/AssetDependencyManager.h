#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <mutex>

#include "Luma/Asset/AssetId.h"
#include "Luma/Asset/AssetType.h"
#include "Luma/Core/Types.h"

namespace Luma {

// Forward declarations
class AssetRegistry;

/**
 * Asset dependency information
 */
struct AssetDependency {
    AssetId dependentAsset;      // Asset that depends on another
    AssetId dependencyAsset;     // Asset that is depended upon
    std::string dependencyType; // Type of dependency (e.g., "texture", "material")
    bool isHardDependency;      // If true, dependency is required
    bool isRuntimeDependency;   // If true, dependency is needed at runtime
};

/**
 * Dependency change notification
 */
struct DependencyChangeNotification {
    AssetId changedAsset;        // Asset that changed
    std::vector<AssetId> affectedAssets; // Assets that depend on the changed asset
    std::string changeType;      // Type of change (e.g., "modified", "deleted", "reimported")
};

/**
 * Asset Dependency Manager - tracks and manages asset dependencies
 * Inspired by UE5's asset dependency system and reference viewer
 */
class AssetDependencyManager {
public:
    AssetDependencyManager();
    ~AssetDependencyManager();
    
    /**
     * Initialize the dependency manager
     */
    void Initialize(AssetRegistry* registry);
    
    /**
     * Shutdown the dependency manager
     */
    void Shutdown();
    
    /**
     * Add a dependency relationship
     * @param dependentAsset Asset that depends on another
     * @param dependencyAsset Asset that is depended upon
     * @param dependencyType Type of dependency
     * @param isHardDependency Whether this is a required dependency
     */
    void AddDependency(const AssetId& dependentAsset,
                      const AssetId& dependencyAsset,
                      const std::string& dependencyType = "generic",
                      bool isHardDependency = true);
    
    /**
     * Remove a dependency relationship
     */
    void RemoveDependency(const AssetId& dependentAsset, const AssetId& dependencyAsset);
    
    /**
     * Remove all dependencies for an asset
     */
    void RemoveAllDependencies(const AssetId& assetId);
    
    /**
     * Get all assets that depend on a given asset
     * @param assetId Asset to get dependents for
     * @return List of assets that depend on this asset
     */
    std::vector<AssetId> GetDependents(const AssetId& assetId) const;
    
    /**
     * Get all dependencies of a given asset
     * @param assetId Asset to get dependencies for
     * @return List of assets this asset depends on
     */
    std::vector<AssetId> GetDependencies(const AssetId& assetId) const;
    
    /**
     * Get detailed dependency information
     */
    std::vector<AssetDependency> GetDependencyInfo(const AssetId& assetId) const;
    
    /**
     * Check if an asset has any dependencies
     */
    bool HasDependencies(const AssetId& assetId) const;
    
    /**
     * Check if an asset has any dependents
     */
    bool HasDependents(const AssetId& assetId) const;
    
    /**
     * Check if asset A depends on asset B (directly or indirectly)
     */
    bool DependsOn(const AssetId& assetA, const AssetId& assetB) const;
    
    /**
     * Get all transitive dependencies (dependencies of dependencies)
     */
    std::vector<AssetId> GetTransitiveDependencies(const AssetId& assetId) const;
    
    /**
     * Get all transitive dependents (dependents of dependents)
     */
    std::vector<AssetId> GetTransitiveDependents(const AssetId& assetId) const;
    
    /**
     * Notify that an asset has changed
     * @param changedAsset Asset that changed
     * @param changeType Type of change
     * @return List of affected assets
     */
    std::vector<AssetId> NotifyAssetChanged(const AssetId& changedAsset, 
                                           const std::string& changeType = "modified");
    
    /**
     * Register a callback for dependency change notifications
     */
    void RegisterChangeCallback(std::function<void(const DependencyChangeNotification&)> callback);
    
    /**
     * Unregister a dependency change callback
     */
    void UnregisterChangeCallback(std::function<void(const DependencyChangeNotification&)> callback);
    
    /**
     * Analyze dependencies for an asset
     * This should be called after an asset is imported to discover its dependencies
     */
    void AnalyzeAssetDependencies(const AssetId& assetId);
    
    /**
     * Rebuild dependency graph for all assets
     * Expensive operation, use sparingly
     */
    void RebuildDependencyGraph();
    
    /**
     * Export dependency graph to DOT format for visualization
     */
    std::string ExportDependencyGraphToDOT() const;
    
    /**
     * Find circular dependencies
     */
    std::vector<std::vector<AssetId>> FindCircularDependencies() const;
    
    /**
     * Get statistics about the dependency graph
     */
    struct DependencyStats {
        usize totalAssets = 0;
        usize totalDependencies = 0;
        usize assetsWithDependencies = 0;
        usize assetsWithDependents = 0;
        usize averageDependenciesPerAsset = 0;
        usize maxDependencies = 0;
        AssetId assetWithMostDependencies;
    };
    
    DependencyStats GetStatistics() const;
    
private:
    /**
     * Internal dependency graph structure
     */
    struct DependencyGraph {
        std::unordered_map<AssetId, std::unordered_set<AssetId>> outgoing; // asset -> dependencies
        std::unordered_map<AssetId, std::unordered_set<AssetId>> incoming; // asset -> dependents
        std::unordered_map<AssetId, AssetDependency> dependencyDetails;
    };
    
    /**
     * Perform DFS to find transitive dependencies
     */
    void FindTransitiveDependencies(const AssetId& assetId,
                                   std::unordered_set<AssetId>& visited,
                                   std::vector<AssetId>& result) const;
    
    /**
     * Perform DFS to find transitive dependents
     */
    void FindTransitiveDependents(const AssetId& assetId,
                                 std::unordered_set<AssetId>& visited,
                                 std::vector<AssetId>& result) const;
    
    /**
     * Detect cycles using DFS
     */
    bool DetectCycle(const AssetId& assetId,
                    std::unordered_set<AssetId>& visited,
                    std::unordered_set<AssetId>& recursionStack,
                    std::vector<AssetId>& cycle) const;
    
    // Asset registry for asset information
    AssetRegistry* m_registry = nullptr;
    
    // Dependency graph
    DependencyGraph m_graph;
    mutable std::mutex m_graphMutex;
    
    // Change callbacks
    std::vector<std::function<void(const DependencyChangeNotification&)>> m_changeCallbacks;
    mutable std::mutex m_callbacksMutex;
};

/**
 * Helper class for common dependency operations
 */
class DependencyHelper {
public:
    /**
     * Get all assets that would be affected if a given asset is deleted
     */
    static std::vector<AssetId> GetAffectedAssetsOnDelete(
        const AssetId& assetId,
        AssetDependencyManager* depManager);
    
    /**
     * Check if it's safe to delete an asset
     * @return true if no other assets depend on it
     */
    static bool IsSafeToDelete(const AssetId& assetId, AssetDependencyManager* depManager);
    
    /**
     * Get the deletion impact summary
     */
    struct DeletionImpact {
        usize directDependents = 0;
        usize transitiveDependents = 0;
        std::vector<AssetId> affectedAssets;
        bool hasHardDependencies = false;
    };
    
    static DeletionImpact AnalyzeDeletionImpact(const AssetId& assetId, 
                                             AssetDependencyManager* depManager);
    
    /**
     * Find assets that reference a given asset in their import settings
     */
    static std::vector<AssetId> FindReferencingAssets(
        const AssetId& assetId,
        AssetRegistry* registry);
};

/**
 * Dependency validation - ensures dependency graph is consistent
 */
class DependencyValidator {
public:
    struct ValidationResult {
        bool isValid = true;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::vector<std::vector<AssetId>> circularDependencies;
        std::vector<AssetId> orphanedAssets;  // Assets with no dependencies that should have some
        std::vector<AssetId> missingDependencies;  // Dependencies that don't exist
    };
    
    /**
     * Validate the entire dependency graph
     */
    static ValidationResult Validate(AssetDependencyManager* depManager,
                                   AssetRegistry* registry);
    
    /**
     * Validate dependencies for a specific asset
     */
    static ValidationResult ValidateAsset(const AssetId& assetId,
                                        AssetDependencyManager* depManager,
                                        AssetRegistry* registry);
};

} // namespace Luma