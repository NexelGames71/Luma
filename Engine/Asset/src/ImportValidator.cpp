#include "Luma/Asset/ImportValidator.h"

#include <algorithm>

#include "Luma/Core/Log.h"

namespace Luma {

// ============================================================================
// MeshValidator Implementation
// ============================================================================

MeshValidator::MeshValidator() = default;

ValidationReport MeshValidator::ValidateSource(const std::filesystem::path& sourcePath) const {
    ValidationReport report;
    
    // Basic file existence check
    if (!std::filesystem::exists(sourcePath)) {
        ValidationResult result;
        result.severity = ValidationSeverity::Error;
        result.message = "Source file does not exist: " + sourcePath.string();
        report.AddResult(result);
        return report;
    }
    
    // Mesh-specific validation
    if (!CheckTriangleCount(sourcePath, report)) {
        // report.isValid = false; // Already set by AddResult
    }
    
    if (!CheckVertexCount(sourcePath, report)) {
        // report.isValid = false;
    }
    
    if (!CheckUVs(sourcePath, report)) {
        // report.isValid = false;
    }
    
    if (!CheckNormals(sourcePath, report)) {
        // report.isValid = false;
    }
    
    if (!CheckTangents(sourcePath, report)) {
        // report.isValid = false;
    }
    
    if (!CheckScale(sourcePath, report)) {
        // report.isValid = false;
    }
    
    if (!CheckMaterials(sourcePath, report)) {
        // report.isValid = false;
    }
    
    if (!CheckDegenerateTriangles(sourcePath, report)) {
        // report.isValid = false;
    }
    
    return report;
}

ValidationReport MeshValidator::ValidateImported(const AssetId& assetId,
                                                const std::filesystem::path& nativePath) const {
    ValidationReport report;
    
    (void)assetId;
    
    // Basic file existence check
    if (!std::filesystem::exists(nativePath)) {
        ValidationResult result;
        result.severity = ValidationSeverity::Error;
        result.message = "Imported asset file does not exist: " + nativePath.string();
        report.AddResult(result);
        return report;
    }
    
    report.AddResult({ValidationSeverity::Info, "Imported asset validated: " + nativePath.string()});
    return report;
}

bool MeshValidator::AutoFix(const std::filesystem::path& sourcePath) const {
    (void)sourcePath;
    // Placeholder: In production, implement auto-fix logic
    return false;
}

bool MeshValidator::CheckTriangleCount(const std::filesystem::path& sourcePath, 
                                      ValidationReport& report) const {
    // Placeholder: In production, this would load the mesh and count triangles
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "Triangle count validation not implemented"});
    return true;
}

bool MeshValidator::CheckVertexCount(const std::filesystem::path& sourcePath,
                                   ValidationReport& report) const {
    // Placeholder
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "Vertex count validation not implemented"});
    return true;
}

bool MeshValidator::CheckUVs(const std::filesystem::path& sourcePath,
                             ValidationReport& report) const {
    // Placeholder
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "UV validation not implemented"});
    return true;
}

bool MeshValidator::CheckNormals(const std::filesystem::path& sourcePath,
                                ValidationReport& report) const {
    // Placeholder
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "Normal validation not implemented"});
    return true;
}

bool MeshValidator::CheckTangents(const std::filesystem::path& sourcePath,
                                  ValidationReport& report) const {
    // Placeholder
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "Tangent validation not implemented"});
    return true;
}

bool MeshValidator::CheckScale(const std::filesystem::path& sourcePath,
                              ValidationReport& report) const {
    // Placeholder
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "Scale validation not implemented"});
    return true;
}

bool MeshValidator::CheckMaterials(const std::filesystem::path& sourcePath,
                                  ValidationReport& report) const {
    // Placeholder
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "Material validation not implemented"});
    return true;
}

bool MeshValidator::CheckDegenerateTriangles(const std::filesystem::path& sourcePath,
                                           ValidationReport& report) const {
    // Placeholder
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "Degenerate triangle validation not implemented"});
    return true;
}

// ============================================================================
// TextureValidator Implementation
// ============================================================================

TextureValidator::TextureValidator() = default;

ValidationReport TextureValidator::ValidateSource(const std::filesystem::path& sourcePath) const {
    ValidationReport report;
    
    // Basic file existence check
    if (!std::filesystem::exists(sourcePath)) {
        ValidationResult result;
        result.severity = ValidationSeverity::Error;
        result.message = "Source file does not exist: " + sourcePath.string();
        report.AddResult(result);
        return report;
    }
    
    // Texture-specific validation
    if (!CheckResolution(sourcePath, report)) {
        // report.isValid = false;
    }
    
    if (!CheckPowerOfTwo(sourcePath, report)) {
        // Warning, not error
    }
    
    return report;
}

ValidationReport TextureValidator::ValidateImported(const AssetId& assetId,
                                                   const std::filesystem::path& nativePath) const {
    ValidationReport report;
    
    (void)assetId;
    
    // Basic file existence check
    if (!std::filesystem::exists(nativePath)) {
        ValidationResult result;
        result.severity = ValidationSeverity::Error;
        result.message = "Imported asset file does not exist: " + nativePath.string();
        report.AddResult(result);
        return report;
    }
    
    report.AddResult({ValidationSeverity::Info, "Imported asset validated: " + nativePath.string()});
    return report;
}

bool TextureValidator::AutoFix(const std::filesystem::path& sourcePath) const {
    (void)sourcePath;
    // Placeholder: In production, implement auto-fix logic
    return false;
}

bool TextureValidator::CheckResolution(const std::filesystem::path& sourcePath,
                                        ValidationReport& report) const {
    // Placeholder: In production, this would load the texture and check resolution
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "Resolution validation not implemented"});
    return true;
}

bool TextureValidator::CheckPowerOfTwo(const std::filesystem::path& sourcePath,
                                        ValidationReport& report) const {
    // Placeholder
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "Power-of-two validation not implemented"});
    return true;
}

bool TextureValidator::CheckAspectRatios(const std::filesystem::path& sourcePath,
                                          ValidationReport& report) const {
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "Aspect ratio validation not implemented"});
    return true;
}

bool TextureValidator::CheckChannels(const std::filesystem::path& sourcePath,
                                     ValidationReport& report) const {
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "Channel validation not implemented"});
    return true;
}

bool TextureValidator::CheckFileSize(const std::filesystem::path& sourcePath,
                                     ValidationReport& report) const {
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "File size validation not implemented"});
    return true;
}

bool TextureValidator::CheckFormat(const std::filesystem::path& sourcePath,
                                   ValidationReport& report) const {
    (void)sourcePath;
    report.AddResult({ValidationSeverity::Info, "Format validation not implemented"});
    return true;
}

} // namespace Luma