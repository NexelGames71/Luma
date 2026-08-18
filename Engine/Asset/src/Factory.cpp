#include "Luma/Asset/Factory.h"
#include "Luma/Asset/AssetRegistry.h"
#include "Luma/Asset/AssetMetadata.h"
#include "Luma/Asset/AssetImportManager.h"
#include <algorithm>
#include <mutex>
#include <fstream>

namespace Luma {

// ============================================================================
// AssetFactory Implementation
// ============================================================================

bool AssetFactory::CanImport(const std::filesystem::path& sourcePath) const {
    if (!std::filesystem::exists(sourcePath)) {
        return false;
    }
    return HasSupportedExtension(sourcePath);
}

ImportResult AssetFactory::Reimport(const AssetId& assetId,
                                   const std::string& importSettings) {
    ImportResult result;
    result.status = ImportStatus::Failed;
    result.errorMessage = "Reimport not supported by this factory";
    return result;
}

bool AssetFactory::ValidateSource(const std::filesystem::path& sourcePath) const {
    if (!std::filesystem::exists(sourcePath)) {
        return false;
    }
    
    if (!std::filesystem::is_regular_file(sourcePath)) {
        return false;
    }
    
    // Check if file is readable
    std::ifstream file(sourcePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Check if file is not empty
    file.seekg(0, std::ios::end);
    if (file.tellg() == 0) {
        return false;
    }
    
    return true;
}

std::filesystem::path AssetFactory::GetSourcePath(const AssetId& assetId) const {
    // Default implementation - can be overridden by specific factories
    return {};
}

bool AssetFactory::HasSupportedExtension(const std::filesystem::path& sourcePath) const {
    std::string extension = sourcePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    auto supportedExtensions = GetSupportedExtensions();
    for (const auto& ext : supportedExtensions) {
        std::string lowerExt = ext;
        std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
        if (extension == lowerExt) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// AssetCreationFactory Implementation
// ============================================================================

ImportResult AssetCreationFactory::Import(const std::filesystem::path& sourcePath,
                                         const std::string& importSettings,
                                         const std::filesystem::path& outputDir) {
    ImportResult result;
    result.status = ImportStatus::Failed;
    result.errorMessage = "Creation factories do not support file import";
    return result;
}

// ============================================================================
// ReimportFactory Implementation
// ============================================================================

void ReimportFactory::CleanupDerivedFiles(const AssetId& assetId) {
    // Default implementation - specific factories can override
    // This would typically remove .lmesh, .meta, and other derived files
}

// ============================================================================
// FactoryRegistry Implementation
// ============================================================================

FactoryRegistry& FactoryRegistry::Instance() {
    static FactoryRegistry instance;
    return instance;
}

void FactoryRegistry::RegisterFactory(std::shared_ptr<AssetFactory> factory) {
    if (!factory) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if factory with same name already exists
    for (const auto& existingFactory : m_factories) {
        if (existingFactory->GetFactoryName() == factory->GetFactoryName()) {
            // Replace existing factory
            auto it = std::remove_if(m_factories.begin(), m_factories.end(),
                [&factory](const std::shared_ptr<AssetFactory>& f) {
                    return f->GetFactoryName() == factory->GetFactoryName();
                });
            m_factories.erase(it, m_factories.end());
            break;
        }
    }
    
    m_factories.push_back(factory);
    
    // Sort by priority (higher priority first)
    std::sort(m_factories.begin(), m_factories.end(),
        [](const std::shared_ptr<AssetFactory>& a, const std::shared_ptr<AssetFactory>& b) {
            return a->GetPriority() > b->GetPriority();
        });
}

void FactoryRegistry::UnregisterFactory(const std::string& factoryName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::remove_if(m_factories.begin(), m_factories.end(),
        [&factoryName](const std::shared_ptr<AssetFactory>& f) {
            return f->GetFactoryName() == factoryName;
        });
    
    m_factories.erase(it, m_factories.end());
}

std::shared_ptr<AssetFactory> FactoryRegistry::GetFactory(const std::string& factoryName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& factory : m_factories) {
        if (factory->GetFactoryName() == factoryName) {
            return factory;
        }
    }
    
    return nullptr;
}

std::shared_ptr<AssetFactory> FactoryRegistry::GetFactoryForFile(
    const std::filesystem::path& sourcePath) const {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& factory : m_factories) {
        if (factory->CanImport(sourcePath)) {
            return factory;
        }
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<AssetFactory>> FactoryRegistry::GetFactoriesForType(
    AssetType type) const {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::shared_ptr<AssetFactory>> result;
    for (const auto& factory : m_factories) {
        if (factory->GetAssetType() == type) {
            result.push_back(factory);
        }
    }
    
    return result;
}

const std::vector<std::shared_ptr<AssetFactory>>& FactoryRegistry::GetAllFactories() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_factories;
}

std::vector<std::shared_ptr<AssetCreationFactory>> FactoryRegistry::GetCreationFactories() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::shared_ptr<AssetCreationFactory>> result;
    for (const auto& factory : m_factories) {
        auto creationFactory = std::dynamic_pointer_cast<AssetCreationFactory>(factory);
        if (creationFactory && creationFactory->CanCreateNew()) {
            result.push_back(creationFactory);
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<ReimportFactory>> FactoryRegistry::GetReimportFactories() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::shared_ptr<ReimportFactory>> result;
    for (const auto& factory : m_factories) {
        auto reimportFactory = std::dynamic_pointer_cast<ReimportFactory>(factory);
        if (reimportFactory) {
            result.push_back(reimportFactory);
        }
    }
    
    return result;
}

} // namespace Luma