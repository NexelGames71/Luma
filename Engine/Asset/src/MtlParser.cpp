#include "Luma/Asset/MtlParser.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace Luma {

namespace {

// Splits a line into whitespace-separated tokens, dropping leading/trailing
// quotes around texture paths and skipping `-option` tokens (e.g. `-s 1 1 1`
// before a map filename) — the last remaining token is the path.
std::vector<std::string> Tokens(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) {
        if (tok.empty() || tok.front() == '-') continue;  // options, comments
        // Strip surrounding quotes (MTL paths may be "quoted with spaces").
        if (tok.size() >= 2 && tok.front() == '"' && tok.back() == '"') {
            tok = tok.substr(1, tok.size() - 2);
        }
        out.push_back(tok);
    }
    return out;
}

bool ParseVec3(const std::vector<std::string>& t, Math::Vec3& out) {
    // t[0] is the directive key ("kd"); values follow at [1..3].
    if (t.size() < 4) return false;
    out.x = std::stof(t[1]);
    out.y = std::stof(t[2]);
    out.z = std::stof(t[3]);
    return true;
}

}  // namespace

std::vector<MtlMaterial> ParseMtlFile(const std::filesystem::path& path) {
    std::vector<MtlMaterial> materials;
    std::ifstream file(path);
    if (!file) return materials;

    std::string line;
    MtlMaterial* cur = nullptr;
    auto ensure = [&]() -> MtlMaterial* {
        if (!cur) {
            materials.emplace_back();
            cur = &materials.back();
        }
        return cur;
    };
    auto last = [](const std::vector<std::string>& t) -> const std::string& {
        static const std::string kEmpty;
        return t.empty() ? kEmpty : t.back();
    };

    while (std::getline(file, line)) {
        // Strip trailing CR (Windows line endings) and inline `#` comments
        // (keep `#` inside quoted paths untouched — rare, acceptable).
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        if (line.empty()) continue;

        auto t = Tokens(line);
        if (t.empty()) continue;
        std::string& key = t[0];
        for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (key == "newmtl") {
            materials.emplace_back();
            cur = &materials.back();
            if (t.size() > 1) cur->name = t[1];
            continue;
        }
        MtlMaterial* m = ensure();
        if (key == "kd") { Math::Vec3 v; if (ParseVec3(t, v)) m->diffuse = v; }
        else if (key == "ks") { Math::Vec3 v; if (ParseVec3(t, v)) m->specular = v; }
        else if (key == "ke") { Math::Vec3 v; if (ParseVec3(t, v)) m->emissive = v; }
        else if (key == "ns" && t.size() > 1) m->shininess = std::stof(t[1]);
        else if (key == "d" && t.size() > 1) m->opacity = std::stof(t[1]);
        else if (key == "tr" && t.size() > 1) m->opacity = 1.0f - std::stof(t[1]);
        else if (key == "map_kd") m->mapDiffuse = last(t);
        else if (key == "map_ks") m->mapSpecular = last(t);
        else if (key == "map_ns") m->mapShininess = last(t);
        else if (key == "map_bump" || key == "bump" || key == "map_kn")
            m->mapNormal = last(t);
        else if (key == "map_pr") m->mapRoughness = last(t);
        else if (key == "map_pm") m->mapMetallic = last(t);
        else if (key == "map_d") m->mapAlpha = last(t);
        else if (key == "map_ke") m->mapEmissive = last(t);
    }
    return materials;
}

std::filesystem::path FindMtlForObj(const std::filesystem::path& objPath) {
    const std::filesystem::path folder = objPath.parent_path();
    std::ifstream file(objPath);
    if (!file) return {};

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        auto t = Tokens(line);
        if (t.empty()) continue;
        std::string key = t[0];
        for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (key != "mtllib" || t.size() < 2) continue;

        // The mtllib path may be nested (e.g. "materials/wood.mtl") or
        // absolute; resolve against the .obj's folder.
        std::filesystem::path rel(t[1]);
        std::filesystem::path candidate = rel.is_absolute() ? rel : folder / rel;
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec)) return candidate;
        // Fall back to the bare filename beside the .obj.
        candidate = folder / rel.filename();
        if (std::filesystem::exists(candidate, ec)) return candidate;
    }

    // No mtllib directive: try the conventional sibling name.
    std::error_code ec;
    std::filesystem::path sibling = objPath;
    sibling.replace_extension(".mtl");
    if (std::filesystem::exists(sibling, ec)) return sibling;
    return {};
}

}  // namespace Luma
