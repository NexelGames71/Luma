#pragma once

#include <string>

#include "Luma/Math/Math.h"
#include "Luma/RHI/Renderer.h"
#include "Luma/Asset/AssetId.h"

// Core scene components stored in the ECS registry. More components (Camera,
// Light, Collider, ...) are added as their subsystems come online.

namespace Luma {

struct NameComponent {
    std::string name;
};

struct TransformComponent {
    Math::Vec3 position{0.0f, 0.0f, 0.0f};
    Math::Vec3 rotationEuler{0.0f, 0.0f, 0.0f};  // degrees
    Math::Vec3 scale{1.0f, 1.0f, 1.0f};

    Math::Mat4 Matrix() const {
        using namespace Math;
        return Translate(position) * RotateY(Radians(rotationEuler.y)) *
               RotateX(Radians(rotationEuler.x)) *
               RotateZ(Radians(rotationEuler.z)) * Scale(scale);
    }
};

// Marks an entity as drawing a mesh with a PBR material.
// Can use built-in primitives or loaded mesh assets.
struct MeshRendererComponent {
    // Built-in primitive (used if meshAsset is invalid)
    MeshPrimitive primitive = MeshPrimitive::Cube;
    
    // Mesh asset reference (invalid ID means use primitive)
    AssetId meshAsset;
    
    // Material properties
    Math::Vec3 albedo{0.82f, 0.82f, 0.85f};  // base color (linear)
    f32 metallic = 0.0f;
    f32 roughness = 0.5f;
    
    // Check if using mesh asset or primitive
    bool UsesMeshAsset() const { return meshAsset.IsValid(); }
};

// A camera. Its position/orientation come from the entity's Transform; this
// component supplies the projection. `primary` marks the camera the game view
// renders through (the editor keeps its own separate fly camera).
enum class ProjectionType { Perspective, Orthographic };

struct CameraComponent {
    ProjectionType projection = ProjectionType::Perspective;
    f32 fovYDegrees = 60.0f;   // perspective vertical field of view
    f32 orthoHeight = 10.0f;   // orthographic view height in world units
    f32 nearZ = 0.1f;
    f32 farZ = 1000.0f;
    bool primary = true;

    // Vulkan clip (0..1 depth, Y-flipped for perspective) — see Math::Perspective.
    Math::Mat4 ProjectionMatrix(f32 aspect) const {
        using namespace Math;
        if (projection == ProjectionType::Orthographic) {
            f32 h = orthoHeight * 0.5f;
            f32 w = h * aspect;
            return Ortho(-w, w, -h, h, nearZ, farZ);
        }
        return Perspective(Radians(fovYDegrees), aspect, nearZ, farZ);
    }
};

// A light source. Position comes from the entity's Transform; the aim
// (spot/directional) comes from its rotation. A Directional light is the
// scene's sun: the scene builder uses its direction/color/intensity + disk
// settings to drive the sky atmosphere, the sun disk, and the directional
// light in every renderer (forward + deferred).
enum class LightType { Directional, Point, Spot, Tube };

struct LightComponent {
    LightType type = LightType::Directional;
    Math::Vec3 color{1.0f, 0.96f, 0.9f};
    f32 intensity = 3.0f;        // lighting scale (sun: also sky + IBL)
    f32 range = 14.0f;           // point/spot falloff distance
    f32 innerAngleDeg = 22.0f;   // spot: full-bright cone half-angle
    f32 outerAngleDeg = 30.0f;   // spot: cutoff half-angle

    // --- Sun disk (directional only) ---
    f32 sunDiskSizeDeg = 1.5f;   // sun angular diameter
    f32 sunDiskIntensity = 1.0f; // sun-disk brightness multiplier

    // --- Attenuation (point/spot/tube; physical inverse-square falloff) ---
    f32 attenuationRadius = 14.0f;  // radius where the light reaches zero
    f32 attenuationPower = 2.0f;    // falloff exponent (2 = inverse square)
    f32 length = 2.0f;              // tube: emitter length (world units)

    // --- Shadows ---
    bool castShadows = true;
    i32 shadowMapSize = 2048;    // shadow map resolution (per cascade for sun)
    f32 shadowBias = 0.0015f;    // depth bias (scaled by cascade distance)
    f32 normalBias = 0.02f;      // normal-offset bias (shadow acne on slopes)
    f32 shadowSoftness = 1.6f;   // PCSS penumbra size (bigger = softer)

    // --- Cascaded shadow maps (directional only) ---
    i32 cascadeCount = 4;        // 1..4 frustum-split cascades
    f32 shadowDistance = 80.0f;  // max world distance the cascades cover
    f32 cascadeSplitLambda = 0.6f;  // 0 = uniform splits, 1 = logarithmic
};

// Drives the world's sky/atmosphere and ambient lighting. Attached by default
// to an Environment game object. The renderer evaluates a physically based
// single-scattering atmosphere (Rayleigh + Mie + ozone) from these values (see
// Luma::SkyParams / the Vulkan sky pass) — Unreal-SkyAtmosphere-style controls.
// The sun itself lives on a directional LightComponent; the scene builder wires
// that light's direction/color/disk into the sky here.
struct EnvironmentComponent {
    // --- Atmosphere (physical single scattering) ---
    bool skyEnabled = true;
    Math::Vec3 rayleighScattering{5.8e-6f, 13.6e-6f, 33.1e-6f};  // per meter
    f32 rayleighScaleHeight = 8000.0f;   // meters
    f32 mieScattering = 3.996e-6f;       // per meter
    f32 mieAbsorption = 4.4e-6f;         // per meter
    f32 mieScaleHeight = 1200.0f;        // meters
    f32 mieAnisotropy = 0.8f;            // Henyey-Greenstein phase g
    f32 ozoneScale = 1.0f;               // ozone layer density multiplier

    // --- Sky post-processing ---
    f32 skyIntensity = 1.0f;             // overall sky brightness
    f32 saturation = 1.25f;              // saturation boost
    f32 exposure = 2.4f;                 // exposure multiplier
    Math::Vec3 skyTint{1.0f, 1.0f, 1.0f};

    // --- Ground & ambient ---
    Math::Vec3 groundColor{0.11f, 0.12f, 0.14f};   // below the horizon
    f32 iblIntensity = 1.0f;                       // ambient / image-based light
};

}  // namespace Luma
