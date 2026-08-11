#pragma once

#include <string>

#include "Luma/Math/Math.h"
#include "Luma/RHI/Renderer.h"

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

// Marks an entity as drawing a built-in primitive mesh with a PBR material.
// Mesh/material asset references replace these as the asset system lands.
struct MeshRendererComponent {
    MeshPrimitive primitive = MeshPrimitive::Cube;
    Math::Vec3 albedo{0.82f, 0.82f, 0.85f};  // base color (linear)
    f32 metallic = 0.0f;
    f32 roughness = 0.5f;
};

// A punctual light source. Position comes from the entity's Transform; the aim
// (spot/directional) comes from its rotation. The Environment already provides
// the sun, so these are additional lights.
enum class LightType { Directional, Point, Spot };

struct LightComponent {
    LightType type = LightType::Point;
    Math::Vec3 color{1.0f, 0.95f, 0.85f};
    f32 intensity = 8.0f;
    f32 range = 14.0f;         // point/spot falloff distance
    f32 innerAngleDeg = 22.0f;  // spot: full-bright cone half-angle
    f32 outerAngleDeg = 30.0f;  // spot: cutoff half-angle
};

// Drives the world's sky/atmosphere. Attached by default to an Environment game
// object. The renderer evaluates an analytic Preetham daylight sky from these
// values (see Luma::SkyParams / the Vulkan sky pass).
struct EnvironmentComponent {
    Math::Vec3 sunDirection{0.35f, 0.65f, 0.55f};  // world dir TO the sun
    Math::Vec3 sunColor{1.0f, 0.96f, 0.9f};        // sun light color
    Math::Vec3 groundColor{0.11f, 0.12f, 0.14f};   // below the horizon
    f32 turbidity = 2.6f;                          // atmospheric haze (1..10)
    f32 sunIntensity = 1.0f;                       // sun-disk brightness
    f32 skyIntensity = 1.0f;                       // overall sky exposure
    f32 sunSizeDegrees = 1.5f;                     // sun angular diameter
    bool skyEnabled = true;
};

}  // namespace Luma
