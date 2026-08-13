#pragma once

#include "Luma/Math/Math.h"
#include "Luma/Scene/Components.h"
#include "Luma/Serialization/Reflection.h"

// Reflection registration for the built-in scene components, plus the SerialTraits
// specialization that lets math vectors serialize as [x, y, z] arrays. This is the
// single place that describes each component's fields (names, categories, ranges),
// and it drives both scene serialization and (later) the data-driven Inspector.

namespace Luma {

// Math::Vec3 <-> JSON array [x, y, z].
template <>
struct SerialTraits<Math::Vec3> {
    static SerialValue Write(const Math::Vec3& v) {
        SerialValue a = SerialValue::MakeArray();
        a.PushBack(static_cast<f64>(v.x));
        a.PushBack(static_cast<f64>(v.y));
        a.PushBack(static_cast<f64>(v.z));
        return a;
    }
    static void Read(const SerialValue& s, Math::Vec3& v) {
        if (s.IsArray() && s.Size() >= 3) {
            v.x = static_cast<f32>(s.At(0).AsFloat(v.x));
            v.y = static_cast<f32>(s.At(1).AsFloat(v.y));
            v.z = static_cast<f32>(s.At(2).AsFloat(v.z));
        }
    }
};

// Per-component descriptors (defined in ComponentReflection.cpp).
template <>
const TypeInfo<NameComponent>& GetTypeInfo<NameComponent>();
template <>
const TypeInfo<TransformComponent>& GetTypeInfo<TransformComponent>();
template <>
const TypeInfo<MeshRendererComponent>& GetTypeInfo<MeshRendererComponent>();
template <>
const TypeInfo<CameraComponent>& GetTypeInfo<CameraComponent>();
template <>
const TypeInfo<LightComponent>& GetTypeInfo<LightComponent>();
template <>
const TypeInfo<EnvironmentComponent>& GetTypeInfo<EnvironmentComponent>();

}  // namespace Luma
