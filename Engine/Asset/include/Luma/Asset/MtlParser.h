#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Luma/Math/Math.h"

// Minimal Wavefront .mtl parser (the material library referenced by .obj
// files). Extracts the classic MTL properties plus the common PBR-oriented
// extensions (map_Pr roughness / map_Pm metallic) so OBJ assets can be
// auto-wired into Luma .lmat materials on import/drop.
//
// Texture paths are stored exactly as written in the file (usually relative
// to the .mtl directory); callers resolve them against the filesystem /
// asset registry.

namespace Luma {

struct MtlMaterial {
    std::string name;                 // `newmtl` name
    Math::Vec3 diffuse{0.8f, 0.8f, 0.8f};  // Kd
    Math::Vec3 specular{0.0f, 0.0f, 0.0f}; // Ks
    Math::Vec3 emissive{0.0f, 0.0f, 0.0f}; // Ke
    f32 shininess = 32.0f;            // Ns (specular exponent)
    f32 opacity = 1.0f;               // d (dissolve; 1 = opaque)
    f32 roughness = -1.0f;            // map_Pr value (PBR ext; -1 = unset)
    f32 metallic = -1.0f;             // map_Pm value (PBR ext; -1 = unset)

    std::string mapDiffuse;   // map_Kd
    std::string mapNormal;    // map_Bump / bump / map_Kn
    std::string mapSpecular;  // map_Ks
    std::string mapShininess; // map_Ns (gloss map)
    std::string mapRoughness; // map_Pr
    std::string mapMetallic;  // map_Pm
    std::string mapAlpha;     // map_d
    std::string mapEmissive;  // map_Ke
};

// Parses a Wavefront .mtl file. Returns the materials in file order (empty
// vector on missing/unreadable file or when no `newmtl` block exists).
std::vector<MtlMaterial> ParseMtlFile(const std::filesystem::path& path);

// Finds the .mtl file referenced by an .obj (`mtllib` directive). Resolves
// relative to the .obj's directory; falls back to `<stem>.mtl` beside the
// .obj. Returns an empty path when nothing exists.
std::filesystem::path FindMtlForObj(const std::filesystem::path& objPath);

}  // namespace Luma
