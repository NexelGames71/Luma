#pragma once

#include <filesystem>
#include <string>

#include "Luma/Material/Material.h"
#include "Luma/Serialization/SerialValue.h"

// Persists a Material as a `.lmat` asset (JSON, via Luma::Serialization).
// The format mirrors the object model: constants, property-input connections,
// and the node graph (ids are persisted so connections survive round-trips).

namespace Luma::Material {

class MaterialSerializer {
public:
    // Saves `material` to `path`. Returns false + `outError` on failure.
    static bool SaveToFile(const Material& material,
                           const std::filesystem::path& path,
                           std::string* outError = nullptr);

    // Loads `path` into `out` (replacing its contents). Returns false +
    // `outError` on failure or malformed JSON.
    static bool LoadFromFile(Material& out, const std::filesystem::path& path,
                             std::string* outError = nullptr);

    static SerialValue ToJson(const Material& material);
    static bool FromJson(Material& out, const SerialValue& value,
                         std::string* outError = nullptr);
};

}  // namespace Luma::Material
