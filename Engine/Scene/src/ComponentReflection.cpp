#include "Luma/Scene/ComponentReflection.h"

namespace Luma {

template <>
const TypeInfo<NameComponent>& GetTypeInfo<NameComponent>() {
    static const TypeInfo<NameComponent> info = [] {
        TypeBuilder<NameComponent> b("Name");
        b.Property("name", &NameComponent::name).Category("General");
        return b.Build();
    }();
    return info;
}

template <>
const TypeInfo<TransformComponent>& GetTypeInfo<TransformComponent>() {
    static const TypeInfo<TransformComponent> info = [] {
        TypeBuilder<TransformComponent> b("Transform");
        b.Property("position", &TransformComponent::position).Category("Transform");
        b.Property("rotationEuler", &TransformComponent::rotationEuler)
            .Category("Transform")
            .Tooltip("Euler angles in degrees");
        b.Property("scale", &TransformComponent::scale).Category("Transform");
        return b.Build();
    }();
    return info;
}

template <>
const TypeInfo<MeshRendererComponent>& GetTypeInfo<MeshRendererComponent>() {
    static const TypeInfo<MeshRendererComponent> info = [] {
        TypeBuilder<MeshRendererComponent> b("MeshRenderer");
        b.Property("primitive", &MeshRendererComponent::primitive).Category("Mesh");
        b.Property("albedo", &MeshRendererComponent::albedo).Category("Material");
        b.Property("metallic", &MeshRendererComponent::metallic)
            .Category("Material")
            .Range(0.0, 1.0);
        b.Property("roughness", &MeshRendererComponent::roughness)
            .Category("Material")
            .Range(0.0, 1.0);
        return b.Build();
    }();
    return info;
}

template <>
const TypeInfo<CameraComponent>& GetTypeInfo<CameraComponent>() {
    static const TypeInfo<CameraComponent> info = [] {
        TypeBuilder<CameraComponent> b("Camera");
        b.Property("projection", &CameraComponent::projection).Category("Camera");
        b.Property("fovYDegrees", &CameraComponent::fovYDegrees)
            .Category("Camera")
            .Range(10.0, 170.0);
        b.Property("orthoHeight", &CameraComponent::orthoHeight).Category("Camera");
        b.Property("nearZ", &CameraComponent::nearZ).Category("Camera");
        b.Property("farZ", &CameraComponent::farZ).Category("Camera");
        b.Property("primary", &CameraComponent::primary).Category("Camera");
        return b.Build();
    }();
    return info;
}

template <>
const TypeInfo<LightComponent>& GetTypeInfo<LightComponent>() {
    static const TypeInfo<LightComponent> info = [] {
        TypeBuilder<LightComponent> b("Light");
        b.Property("type", &LightComponent::type).Category("Light");
        b.Property("color", &LightComponent::color).Category("Light");
        b.Property("intensity", &LightComponent::intensity)
            .Category("Light")
            .Range(0.0, 100.0);
        b.Property("range", &LightComponent::range).Category("Light");
        b.Property("innerAngleDeg", &LightComponent::innerAngleDeg)
            .Category("Spot")
            .Range(0.0, 90.0);
        b.Property("outerAngleDeg", &LightComponent::outerAngleDeg)
            .Category("Spot")
            .Range(0.0, 90.0);
        return b.Build();
    }();
    return info;
}

template <>
const TypeInfo<EnvironmentComponent>& GetTypeInfo<EnvironmentComponent>() {
    static const TypeInfo<EnvironmentComponent> info = [] {
        TypeBuilder<EnvironmentComponent> b("Environment");
        b.Property("sunDirection", &EnvironmentComponent::sunDirection).Category("Sky");
        b.Property("sunColor", &EnvironmentComponent::sunColor).Category("Sky");
        b.Property("groundColor", &EnvironmentComponent::groundColor).Category("Sky");
        b.Property("turbidity", &EnvironmentComponent::turbidity)
            .Category("Sky")
            .Range(1.0, 10.0);
        b.Property("sunIntensity", &EnvironmentComponent::sunIntensity).Category("Sky");
        b.Property("skyIntensity", &EnvironmentComponent::skyIntensity).Category("Sky");
        b.Property("sunSizeDegrees", &EnvironmentComponent::sunSizeDegrees).Category("Sky");
        b.Property("skyEnabled", &EnvironmentComponent::skyEnabled).Category("Sky");
        return b.Build();
    }();
    return info;
}

}  // namespace Luma
