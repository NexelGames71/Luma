#pragma once

#include <cstdint>

// Luma's port of UE5.8's EMaterialValueType (Runtime/Engine/Public/MaterialValueType.h).
//
// Value types are a bitmask so a pin can declare "any float" (MCT_Numeric),
// "any texture" (MCT_Texture), or a single concrete type (MCT_Float3). Pin
// compatibility is decided by CanConnectMaterialValueTypes, the direct port
// of UE's rule (see MaterialExpressions.cpp): unknown plugs into anything,
// overlapping bits connect, and any two numeric types connect (scalars
// auto-splat, vectors truncate/pad in the compiler).

namespace Luma::Material {

enum MaterialValueType : u32 {
    MCT_Float1       = 1u << 0,  // float
    MCT_Float2       = 1u << 1,  // vec2
    MCT_Float3       = 1u << 2,  // vec3
    MCT_Float4       = 1u << 3,  // vec4

    MCT_Texture2D    = 1u << 4,  // sampler2D
    MCT_StaticBool   = 1u << 5,  // compile-time bool
    MCT_Unknown      = 1u << 6,  // dynamic / unconnected: anything plugs in
    MCT_Int          = 1u << 7,  // integer constant (numerically compatible)
    MCT_Menu         = 1u << 8,  // menu value (only connects to menu sockets)

    // Composites (UE MaterialValueType.h).
    MCT_Texture = MCT_Texture2D,
    MCT_Float   = MCT_Float1 | MCT_Float2 | MCT_Float3 | MCT_Float4,
    MCT_Numeric = MCT_Float | MCT_StaticBool | MCT_Int,
    MCT_None    = 0,
};

// UE's CanConnectMaterialValueTypes — the exact algorithm from
// MaterialExpressions.cpp (exec pins are not part of Luma's set yet, so that
// branch is omitted).
inline bool CanConnectMaterialValueTypes(MaterialValueType input,
                                         MaterialValueType output) {
    if (input & MCT_Unknown) {
        // can plug anything into unknown inputs
        return true;
    }
    if (output & MCT_Unknown) {
        // unknown outputs connect to everything (workflow ease)
        return true;
    }
    if (input & output) {
        return true;
    }
    // both numeric: connect (scalar auto-promotes, vectors cast)
    if ((input & MCT_Numeric) && (output & MCT_Numeric)) {
        return true;
    }
    return false;
}

// Number of float components of a single float value type (MCT_Float1 -> 1).
inline int ValueTypeComponentCount(MaterialValueType t) {
    switch (t) {
        case MCT_Float4: return 4;
        case MCT_Float3: return 3;
        case MCT_Float2: return 2;
        default: return 1;
    }
}

// Short name for debug/log (UE: "float1".."float4", "texture2d", "bool").
inline const char* ValueTypeName(MaterialValueType t) {
    switch (t) {
        case MCT_Float1: return "float";
        case MCT_Float2: return "vec2";
        case MCT_Float3: return "vec3";
        case MCT_Float4: return "vec4";
        case MCT_Texture2D: return "texture2d";
        case MCT_StaticBool: return "static bool";
        case MCT_Int: return "int";
        case MCT_Menu: return "menu";
        case MCT_Unknown: return "unknown";
        default: return "none";
    }
}

// GLSL scalar/vector type name for a single concrete float type.
inline const char* ValueTypeGlsl(MaterialValueType t) {
    switch (t) {
        case MCT_Float2: return "vec2";
        case MCT_Float3: return "vec3";
        case MCT_Float4: return "vec4";
        default: return "float";
    }
}

}  // namespace Luma::Material
