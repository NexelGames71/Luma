#include "Luma/Core/Config.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>

namespace Luma {
namespace {

std::string_view Trim(std::string_view s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    auto begin = std::find_if(s.begin(), s.end(), notSpace);
    auto end = std::find_if(s.rbegin(), s.rend(), notSpace).base();
    if (begin >= end) return {};
    return s.substr(static_cast<usize>(begin - s.begin()),
                    static_cast<usize>(end - begin));
}

std::string ToLower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

}  // namespace

bool Config::LoadFromString(std::string_view text) {
    bool ok = true;
    std::string section;
    std::istringstream stream{std::string(text)};
    std::string rawLine;
    while (std::getline(stream, rawLine)) {
        std::string_view line = Trim(rawLine);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }
        if (line.front() == '[') {
            auto close = line.find(']');
            if (close == std::string_view::npos) {
                ok = false;
                continue;
            }
            section = std::string(Trim(line.substr(1, close - 1)));
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string_view::npos) {
            ok = false;
            continue;
        }
        std::string_view keyPart = Trim(line.substr(0, eq));
        std::string_view valuePart = Trim(line.substr(eq + 1));
        if (keyPart.empty()) {
            ok = false;
            continue;
        }
        std::string fullKey =
            section.empty() ? std::string(keyPart)
                            : section + "." + std::string(keyPart);
        m_values[fullKey] = std::string(valuePart);
    }
    return ok;
}

bool Config::LoadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) return false;
    std::ostringstream ss;
    ss << file.rdbuf();
    return LoadFromString(ss.str());
}

bool Config::Has(std::string_view key) const {
    return m_values.find(std::string(key)) != m_values.end();
}

void Config::Set(std::string_view key, std::string value) {
    m_values[std::string(key)] = std::move(value);
}

std::string Config::GetString(std::string_view key,
                              std::string_view fallback) const {
    auto it = m_values.find(std::string(key));
    return it != m_values.end() ? it->second : std::string(fallback);
}

i32 Config::GetInt(std::string_view key, i32 fallback) const {
    auto it = m_values.find(std::string(key));
    if (it == m_values.end()) return fallback;
    const std::string& v = it->second;
    i32 result = 0;
    auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), result);
    return ec == std::errc{} ? result : fallback;
}

f32 Config::GetFloat(std::string_view key, f32 fallback) const {
    auto it = m_values.find(std::string(key));
    if (it == m_values.end()) return fallback;
    // std::from_chars for float has spotty support on some stdlibs; use strtof.
    const std::string& v = it->second;
    char* end = nullptr;
    float result = std::strtof(v.c_str(), &end);
    return end != v.c_str() ? result : fallback;
}

bool Config::GetBool(std::string_view key, bool fallback) const {
    auto it = m_values.find(std::string(key));
    if (it == m_values.end()) return fallback;
    std::string v = ToLower(it->second);
    if (v == "true" || v == "1" || v == "yes" || v == "on") return true;
    if (v == "false" || v == "0" || v == "no" || v == "off") return false;
    return fallback;
}

}  // namespace Luma
