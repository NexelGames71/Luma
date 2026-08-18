#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <mutex>

#include "Luma/Asset/AssetType.h"
#include "Luma/Asset/AssetId.h"
#include "Luma/Core/Types.h"

namespace Luma {

/**
 * Validation severity level
 */
enum class ValidationSeverity {
    Info,       // Informational message
    Warning,    // Warning that doesn't prevent import
    Error,      // Error that prevents import
    Critical    // Critical error that must be fixed
};

/**
 * Validation result for a single check
 */
struct ValidationResult {
    ValidationSeverity severity = ValidationSeverity::Info;
    std::string message;
    std::string checkName;
    std::string suggestion;  // Suggested fix for the issue
    bool canAutoFix = false; // Whether the validator can automatically fix this
};

/**
 * Complete validation report
 */
struct ValidationReport {
    bool isValid = true;
    std::vector<ValidationResult> results;
    usize errorCount = 0;
    usize warningCount = 0;
    usize infoCount = 0;
    
    void AddResult(const ValidationResult& result) {
        results.push_back(result);
        
        switch (result.severity) {
            case ValidationSeverity::Error:
            case ValidationSeverity::Critical:
                errorCount++;
                isValid = false;
                break;
            case ValidationSeverity::Warning:
                warningCount++;
                break;
            case ValidationSeverity::Info:
                infoCount++;
                break;
        }
    }
    
    std::vector<ValidationResult> GetErrors() const {
        std::vector<ValidationResult> errors;
        for (const auto& result : results) {
            if (result.severity == ValidationSeverity::Error || 
                result.severity == ValidationSeverity::Critical) {
                errors.push_back(result);
            }
        }
        return errors;
    }
    
    std::vector<ValidationResult> GetWarnings() const {
        std::vector<ValidationResult> warnings;
        for (const auto& result : results) {
            if (result.severity == ValidationSeverity::Warning) {
                warnings.push_back(result);
            }
        }
        return warnings;
    }
    
    std::string GetSummary() const {
        return std::to_string(errorCount) + " errors, " + 
               std::to_string(warningCount) + " warnings, " +
               std::to_string(infoCount) + " info";
    }
};

/**
 * Base class for asset validators
 * Inspired by UE5's asset validation system
 */
class AssetValidator {
public:
    virtual ~AssetValidator() = default;
    
    /**
     * Get the asset type this validator handles
     */
    virtual AssetType GetAssetType() const = 0;
    
    /**
     * Get the validator name
     */
    virtual std::string GetName() const = 0;
    
    /**
     * Get the validator description
     */
    virtual std::string GetDescription() const { return "Asset Validator"; }
    
    /**
     * Validate a source file before import
     * @param sourcePath Path to the source file
     * @return Validation report
     */
    virtual ValidationReport ValidateSource(const std::filesystem::path& sourcePath) const = 0;
    
    /**
     * Validate the imported asset after import
     * @param assetId ID of the imported asset
     * @param nativePath Path to the native asset file
     * @return Validation report
     */
    virtual ValidationReport ValidateImported(const AssetId& assetId,
                                             const std::filesystem::path& nativePath) const = 0;
    
    /**
     * Check if this validator can auto-fix issues
     */
    virtual bool CanAutoFix() const { return false; }
    
    /**
     * Attempt to auto-fix validation issues
     * @param sourcePath Path to the source file
     * @return true if fixes were applied successfully
     */
    virtual bool AutoFix(const std::filesystem::path& sourcePath) const {
        return false;
    }
    
    /**
     * Get the validator priority (higher priority validators run first)
     */
    virtual i32 GetPriority() const { return 0; }
};

/**
 * Mesh validator - validates 3D mesh assets
 */
class MeshValidator : public AssetValidator {
public:
    MeshValidator();
    ~MeshValidator() override = default;
    
    AssetType GetAssetType() const override { return AssetType::Mesh; }
    std::string GetName() const override { return "MeshValidator"; }
    std::string GetDescription() const override { return "Validates 3D mesh assets for common issues"; }
    
    ValidationReport ValidateSource(const std::filesystem::path& sourcePath) const override;
    ValidationReport ValidateImported(const AssetId& assetId,
                                     const std::filesystem::path& nativePath) const override;
    bool CanAutoFix() const override { return true; }
    bool AutoFix(const std::filesystem::path& sourcePath) const override;
    i32 GetPriority() const override { return 100; }
    
private:
    // Validation checks
    bool CheckTriangleCount(const std::filesystem::path& sourcePath, 
                          ValidationReport& report) const;
    bool CheckVertexCount(const std::filesystem::path& sourcePath,
                         ValidationReport& report) const;
    bool CheckUVs(const std::filesystem::path& sourcePath,
                 ValidationReport& report) const;
    bool CheckNormals(const std::filesystem::path& sourcePath,
                    ValidationReport& report) const;
    bool CheckTangents(const std::filesystem::path& sourcePath,
                      ValidationReport& report) const;
    bool CheckScale(const std::filesystem::path& sourcePath,
                   ValidationReport& report) const;
    bool CheckMaterials(const std::filesystem::path& sourcePath,
                      ValidationReport& report) const;
    bool CheckDegenerateTriangles(const std::filesystem::path& sourcePath,
                                ValidationReport& report) const;
    
    // Configuration
    static constexpr usize kMaxTriangleCount = 1000000;  // 1M triangles
    static constexpr usize kMaxVertexCount = 500000;     // 500K vertices
    static constexpr f32 kMinScale = 0.001f;
    static constexpr f32 kMaxScale = 1000.0f;
};

/**
 * Texture validator - validates texture assets
 */
class TextureValidator : public AssetValidator {
public:
    TextureValidator();
    ~TextureValidator() override = default;
    
    AssetType GetAssetType() const override { return AssetType::Texture; }
    std::string GetName() const override { return "TextureValidator"; }
    std::string GetDescription() const override { return "Validates texture assets for common issues"; }
    
    ValidationReport ValidateSource(const std::filesystem::path& sourcePath) const override;
    ValidationReport ValidateImported(const AssetId& assetId,
                                     const std::filesystem::path& nativePath) const override;
    bool CanAutoFix() const override { return true; }
    bool AutoFix(const std::filesystem::path& sourcePath) const override;
    i32 GetPriority() const override { return 100; }
    
private:
    // Validation checks
    bool CheckResolution(const std::filesystem::path& sourcePath,
                       ValidationReport& report) const;
    bool CheckPowerOfTwo(const std::filesystem::path& sourcePath,
                        ValidationReport& report) const;
    bool CheckAspectRatios(const std::filesystem::path& sourcePath,
                          ValidationReport& report) const;
    bool CheckChannels(const std::filesystem::path& sourcePath,
                     ValidationReport& report) const;
    bool CheckFileSize(const std::filesystem::path& sourcePath,
                     ValidationReport& report) const;
    bool CheckFormat(const std::filesystem::path& sourcePath,
                   ValidationReport& report) const;
    
    // Configuration
    static constexpr i32 kMaxResolution = 16384;  // 16K
    static constexpr i32 kRecommendedMaxResolution = 4096;  // 4K
    static constexpr usize kMaxFileSize = 100 * 1024 * 1024;  // 100MB
};

/**
 * Import validation manager - coordinates all validators
 */
class ImportValidationManager {
public:
    static ImportValidationManager& Instance();
    
    /**
     * Register a validator
     */
    void RegisterValidator(std::shared_ptr<AssetValidator> validator);
    
    /**
     * Unregister a validator
     */
    void UnregisterValidator(const std::string& validatorName);
    
    /**
     * Get validator by name
     */
    std::shared_ptr<AssetValidator> GetValidator(const std::string& validatorName) const;
    
    /**
     * Get all validators for a specific asset type
     */
    std::vector<std::shared_ptr<AssetValidator>> GetValidatorsForType(AssetType type) const;
    
    /**
     * Validate a source file before import
     * @param sourcePath Path to the source file
     * @param assetType Type of asset
     * @return Combined validation report from all applicable validators
     */
    ValidationReport ValidateSource(const std::filesystem::path& sourcePath, 
                                   AssetType assetType) const;
    
    /**
     * Validate an imported asset
     * @param assetId ID of the imported asset
     * @param nativePath Path to the native asset file
     * @param assetType Type of asset
     * @return Combined validation report from all applicable validators
     */
    ValidationReport ValidateImported(const AssetId& assetId,
                                    const std::filesystem::path& nativePath,
                                    AssetType assetType) const;
    
    /**
     * Attempt to auto-fix validation issues
     * @param sourcePath Path to the source file
     * @param assetType Type of asset
     * @return true if any fixes were applied
     */
    bool AutoFix(const std::filesystem::path& sourcePath, AssetType assetType) const;
    
    /**
     * Enable or disable a specific validator
     */
    void SetValidatorEnabled(const std::string& validatorName, bool enabled);
    
    /**
     * Check if a validator is enabled
     */
    bool IsValidatorEnabled(const std::string& validatorName) const;
    
    /**
     * Get all registered validators
     */
    const std::vector<std::shared_ptr<AssetValidator>>& GetAllValidators() const;
    
private:
    ImportValidationManager() = default;
    
    std::vector<std::shared_ptr<AssetValidator>> m_validators;
    std::unordered_map<std::string, bool> m_validatorEnabled;
    mutable std::mutex m_mutex;
};

/**
 * Validation helper for common validation scenarios
 */
class ValidationHelper {
public:
    /**
     * Quick validation - returns true if asset passes all critical checks
     */
    static bool QuickValidate(const std::filesystem::path& sourcePath, AssetType assetType);
    
    /**
     * Full validation with detailed report
     */
    static ValidationReport FullValidate(const std::filesystem::path& sourcePath, AssetType assetType);
    
    /**
     * Validate and auto-fix if possible
     */
    static ValidationReport ValidateAndAutoFix(const std::filesystem::path& sourcePath, 
                                               AssetType assetType);
    
    /**
     * Get human-readable validation summary
     */
    static std::string GetValidationSummary(const ValidationReport& report);
    
    /**
     * Check if validation report is acceptable for import
     */
    static bool IsAcceptableForImport(const ValidationReport& report);
};

} // namespace Luma