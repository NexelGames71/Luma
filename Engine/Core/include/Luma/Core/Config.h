#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "Luma/Core/Types.h"

// Minimal INI-style configuration store.
//
//   [Window]
//   width = 1280      ; comment
//   fullscreen = false
//
// Keys are stored fully-qualified as "Section.key". Values are read with typed
// accessors that fall back to a supplied default when the key is missing or the
// stored text cannot be parsed as the requested type.

namespace Luma {

class Config {
public:
    // Parses INI text. Returns false if a syntactically broken line is found
    // (already-parsed values are retained). Later keys overwrite earlier ones.
    bool LoadFromString(std::string_view text);
    bool LoadFromFile(const std::string& path);

    bool Has(std::string_view key) const;
    void Set(std::string_view key, std::string value);

    std::string GetString(std::string_view key,
                          std::string_view fallback = {}) const;
    i32 GetInt(std::string_view key, i32 fallback = 0) const;
    f32 GetFloat(std::string_view key, f32 fallback = 0.0f) const;
    bool GetBool(std::string_view key, bool fallback = false) const;

    usize Size() const { return m_values.size(); }

private:
    std::unordered_map<std::string, std::string> m_values;
};

}  // namespace Luma
