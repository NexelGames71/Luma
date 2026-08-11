#pragma once

#include <string>

#include "Luma/Math/Math.h"

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

// Marks an entity as drawing a mesh. For now the only mesh is the built-in cube;
// a mesh/material asset reference replaces `color` as the asset system lands.
struct MeshRendererComponent {
    Math::Vec3 color{0.80f, 0.80f, 0.85f};
};

// Drives the world's sky/atmosphere. Attached by default to an Environment game
// object. The renderer evaluates an analytic Preetham daylight sky from these
// values (see Luma::SkyParams / the Vulkan sky pass).
struct EnvironmentComponent {
    Math::Vec3 sunDirection{0.35f, 0.65f, 0.55f};  // world dir TO the sun
    Math::Vec3 groundColor{0.11f, 0.12f, 0.14f};   // below the horizon
    f32 turbidity = 2.6f;                          // atmospheric haze (1..10)
    f32 sunIntensity = 1.0f;                       // sun-disk brightness
    f32 skyIntensity = 1.0f;                       // overall sky exposure
    f32 sunSizeDegrees = 1.5f;                     // sun angular diameter
    bool skyEnabled = true;
};

}  // namespace Luma
