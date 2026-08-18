#pragma once

#include <cstdint>

namespace Luma::Material {

// Material properties — the named PBR slots. Node graph removed.
// IMPORTANT: persisted as ints in .lmat files — append only, never reorder.
enum class MaterialProperty : u8 {
    // UE MP_BaseColor / MP_DiffuseColor
    BaseColor = 0,
    // UE MP_Metallic
    Metallic,
    // UE MP_Specular
    Specular,
    // UE MP_Roughness
    Roughness,
    // UE MP_Normal
    Normal,
    // UE MP_Tangent
    Tangent,
    // UE MP_EmissiveColor
    EmissiveColor,
    // UE MP_Opacity
    Opacity,
    // UE MP_OpacityMask
    OpacityMask,
    // UE MP_WorldPositionOffset
    WorldPositionOffset,
    // UE MP_SubsurfaceColor
    SubsurfaceColor,
    // UE MP_AmbientOcclusion
    AmbientOcclusion,
    // UE MP_Refraction
    Refraction,
    // UE MP_PixelDepthOffset
    PixelDepthOffset,

    // --- Blender-style Material Output volume + thickness inputs -----------
    // The Material Output node's Volume section (smoke/fluid shading) and the
    // EEVEE Thickness input. Appended so existing .lmat files stay valid.
    VolumeColor,      // volume scatter/emission color
    VolumeDensity,    // volume density
    VolumeFlame,      // fire density
    VolumeTemperature,// 0..1 maps to 0..1000 kelvin
    Thickness,        // EEVEE SSS thickness approximation

    Count,
};

// Render target for the Material Output node (Blender's Target property).
// Cycles = offline path, EEVEE = real-time path; stored per material.
enum class MaterialTarget : u8 {
    Cycles = 0,
    EEVEE = 1,
    Count,
};

// Human-readable label for a material property (the pin label on the
// Material Output node, e.g. "Base Color").
inline const char* MaterialPropertyName(MaterialProperty p) {
    switch (p) {
        case MaterialProperty::BaseColor: return "Base Color";
        case MaterialProperty::Metallic: return "Metallic";
        case MaterialProperty::Specular: return "Specular";
        case MaterialProperty::Roughness: return "Roughness";
        case MaterialProperty::Normal: return "Normal";
        case MaterialProperty::Tangent: return "Tangent";
        case MaterialProperty::EmissiveColor: return "Emissive Color";
        case MaterialProperty::Opacity: return "Opacity";
        case MaterialProperty::OpacityMask: return "Opacity Mask";
        case MaterialProperty::WorldPositionOffset:
            return "World Position Offset";
        case MaterialProperty::SubsurfaceColor:
            return "Subsurface Color";
        case MaterialProperty::AmbientOcclusion:
            return "Ambient Occlusion";
        case MaterialProperty::Refraction: return "Refraction";
        case MaterialProperty::VolumeColor: return "Volume Color";
        case MaterialProperty::VolumeDensity: return "Volume Density";
        case MaterialProperty::VolumeFlame: return "Volume Flame";
        case MaterialProperty::VolumeTemperature:
            return "Volume Temperature";
        case MaterialProperty::Thickness: return "Thickness";
        case MaterialProperty::PixelDepthOffset:
            return "Pixel Depth Offset";
        default: return "";
    }
}

}  // namespace Luma::Material
