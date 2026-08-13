#pragma once

#include <array>
#include <string>
#include <string_view>

#include "Luma/Core/Types.h"

// Coarse asset type taxonomy. The mapping from file extension to type lives
// in AssetType.cpp so a new extension doesn't ripple through includes.

namespace Luma {

enum class AssetType : u8 {
    Unknown = 0,
    Texture,
    Mesh,
    Material,
    Shader,
    Script,
    Prefab,
    Scene,
    Sound,
    Font,
    Folder,  // not a file, but the Content Browser shows directory rows
    Count,
};

const char* AssetTypeName(AssetType t) noexcept;
std::string_view AssetTypeSlug(AssetType t) noexcept;
AssetType AssetTypeFromExtension(std::string_view ext) noexcept;
std::string_view AssetTypeIconHint(AssetType t) noexcept;  // enum name from IconKind

// The set of extensions the registry will index for each type. Editable
// here so adding `.ktx2` or `.glb` is a one-line change.
struct TypeExtensions {
    std::array<std::string_view, 8> texture{"png",  "jpg",  "jpeg", "tga",
                                            "bmp",  "webp", "ktx",  "ktx2"};
    std::array<std::string_view, 4> mesh{"obj", "gltf", "glb", "fbx"};
    std::array<std::string_view, 2> material{"mat", "lumat"};
    std::array<std::string_view, 4> shader{"hlsl", "glsl", "frag", "vert"};
    std::array<std::string_view, 4> script{"lua", "py", "js", "cs"};
    std::array<std::string_view, 2> prefab{"prefab", "lumapfb"};
    std::array<std::string_view, 2> scene{"luma", "scene"};
    std::array<std::string_view, 4> sound{"wav", "ogg", "mp3", "flac"};
    std::array<std::string_view, 2> font{"ttf", "otf"};
};

}  // namespace Luma
