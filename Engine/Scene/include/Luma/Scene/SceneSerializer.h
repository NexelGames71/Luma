#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "Luma/Scene/Scene.h"

// Saves/loads a Scene to Luma's JSON scene format. The document is:
//
//   { "version": 1,
//     "entities": [ { "components": { "Name": {...}, "Transform": {...}, ... } } ] }
//
// Only entities carrying a NameComponent (the canonical scene objects) are
// written. Loading clears the target scene first, then reconstructs entities and
// their components; unknown component keys are ignored so files stay forward- and
// backward-compatible as components evolve.

namespace Luma {

class SceneSerializer {
public:
    static constexpr int kVersion = 1;

    static std::string SaveToString(const Scene& scene, bool pretty = true);
    static bool LoadFromString(Scene& scene, std::string_view json,
                               std::string* outError = nullptr);

    static bool SaveToFile(const Scene& scene, const std::filesystem::path& path,
                           std::string* outError = nullptr);
    static bool LoadFromFile(Scene& scene, const std::filesystem::path& path,
                             std::string* outError = nullptr);
};

}  // namespace Luma
