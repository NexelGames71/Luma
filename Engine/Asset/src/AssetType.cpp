#include "Luma/Asset/AssetType.h"

namespace Luma {

const char* AssetTypeName(AssetType t) noexcept {
    switch (t) {
        case AssetType::Unknown: return "Unknown";
        case AssetType::Texture: return "Texture";
        case AssetType::Mesh: return "Mesh";
        case AssetType::Material: return "Material";
        case AssetType::Shader: return "Shader";
        case AssetType::Script: return "Script";
        case AssetType::Prefab: return "Prefab";
        case AssetType::Scene: return "Scene";
        case AssetType::Sound: return "Sound";
        case AssetType::Font: return "Font";
        case AssetType::Folder: return "Folder";
        default: return "?";
    }
}

std::string_view AssetTypeSlug(AssetType t) noexcept {
    switch (t) {
        case AssetType::Unknown: return "unknown";
        case AssetType::Texture: return "texture";
        case AssetType::Mesh: return "mesh";
        case AssetType::Material: return "material";
        case AssetType::Shader: return "shader";
        case AssetType::Script: return "script";
        case AssetType::Prefab: return "prefab";
        case AssetType::Scene: return "scene";
        case AssetType::Sound: return "sound";
        case AssetType::Font: return "font";
        case AssetType::Folder: return "folder";
        default: return "?";
    }
}

namespace {
template <typename Arr>
bool ExtIn(const std::string_view ext, const Arr& arr) {
    for (auto v : arr) {
        if (v.empty()) break;
        if (v == ext) return true;
    }
    return false;
}
}  // namespace

AssetType AssetTypeFromExtension(std::string_view ext) noexcept {
    if (ext.empty()) return AssetType::Unknown;
    if (ext.front() == '.') ext.remove_prefix(1);
    if (ExtIn(ext, TypeExtensions{}.texture)) return AssetType::Texture;
    if (ExtIn(ext, TypeExtensions{}.mesh)) return AssetType::Mesh;
    if (ExtIn(ext, TypeExtensions{}.material)) return AssetType::Material;
    if (ExtIn(ext, TypeExtensions{}.shader)) return AssetType::Shader;
    if (ExtIn(ext, TypeExtensions{}.script)) return AssetType::Script;
    if (ExtIn(ext, TypeExtensions{}.prefab)) return AssetType::Prefab;
    if (ExtIn(ext, TypeExtensions{}.scene)) return AssetType::Scene;
    if (ExtIn(ext, TypeExtensions{}.sound)) return AssetType::Sound;
    if (ExtIn(ext, TypeExtensions{}.font)) return AssetType::Font;
    return AssetType::Unknown;
}

std::string_view AssetTypeIconHint(AssetType t) noexcept {
    // Names match Luma::Slate::Icon enum values; the Content Browser maps
    // these to icons so we don't pull Slate into the Asset module.
    switch (t) {
        case AssetType::Texture: return "Image";
        case AssetType::Mesh: return "Cube";
        case AssetType::Material: return "Sphere";
        case AssetType::Shader: return "Plane";
        case AssetType::Script: return "Cylinder";
        case AssetType::Prefab: return "Cube";
        case AssetType::Scene: return "Plane";
        case AssetType::Sound: return "Play";
        case AssetType::Font: return "Refresh";
        case AssetType::Folder: return "Folder";
        default: return "Dot";
    }
}

}  // namespace Luma
