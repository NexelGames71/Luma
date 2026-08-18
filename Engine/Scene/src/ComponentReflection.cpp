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
        // Sun disk (directional)
        b.Property("sunDiskSizeDeg", &LightComponent::sunDiskSizeDeg)
            .Category("Sun")
            .Range(0.1, 15.0);
        b.Property("sunDiskIntensity", &LightComponent::sunDiskIntensity)
            .Category("Sun")
            .Range(0.0, 10.0);
        // Attenuation (point/spot/tube)
        b.Property("attenuationRadius", &LightComponent::attenuationRadius)
            .Category("Attenuation");
        b.Property("attenuationPower", &LightComponent::attenuationPower)
            .Category("Attenuation")
            .Range(0.5, 4.0);
        b.Property("length", &LightComponent::length)
            .Category("Attenuation")
            .Range(0.1, 20.0);
        // Shadows
        b.Property("castShadows", &LightComponent::castShadows).Category("Shadows");
        b.Property("shadowMapSize", &LightComponent::shadowMapSize)
            .Category("Shadows")
            .Range(256, 4096);
        b.Property("shadowBias", &LightComponent::shadowBias)
            .Category("Shadows")
            .Range(0.0, 0.05);
        b.Property("normalBias", &LightComponent::normalBias)
            .Category("Shadows")
            .Range(0.0, 1.0);
        b.Property("shadowSoftness", &LightComponent::shadowSoftness)
            .Category("Shadows")
            .Range(0.0, 8.0);
        // Cascaded shadow maps (directional)
        b.Property("cascadeCount", &LightComponent::cascadeCount)
            .Category("Shadows")
            .Range(1, 4);
        b.Property("shadowDistance", &LightComponent::shadowDistance)
            .Category("Shadows");
        b.Property("cascadeSplitLambda", &LightComponent::cascadeSplitLambda)
            .Category("Shadows")
            .Range(0.0, 1.0);
        return b.Build();
    }();
    return info;
}

template <>
const TypeInfo<EnvironmentComponent>& GetTypeInfo<EnvironmentComponent>() {
    static const TypeInfo<EnvironmentComponent> info = [] {
        TypeBuilder<EnvironmentComponent> b("Environment");
        // Atmosphere
        b.Property("skyEnabled", &EnvironmentComponent::skyEnabled).Category("Atmosphere");
        b.Property("rayleighScattering", &EnvironmentComponent::rayleighScattering)
            .Category("Atmosphere");
        b.Property("rayleighScaleHeight", &EnvironmentComponent::rayleighScaleHeight)
            .Category("Atmosphere");
        b.Property("mieScattering", &EnvironmentComponent::mieScattering).Category("Atmosphere");
        b.Property("mieAbsorption", &EnvironmentComponent::mieAbsorption).Category("Atmosphere");
        b.Property("mieScaleHeight", &EnvironmentComponent::mieScaleHeight).Category("Atmosphere");
        b.Property("mieAnisotropy", &EnvironmentComponent::mieAnisotropy)
            .Category("Atmosphere")
            .Range(0.0, 1.0);
        b.Property("ozoneScale", &EnvironmentComponent::ozoneScale).Category("Atmosphere");
        // Sky
        b.Property("skyIntensity", &EnvironmentComponent::skyIntensity).Category("Sky");
        b.Property("saturation", &EnvironmentComponent::saturation).Category("Sky");
        b.Property("exposure", &EnvironmentComponent::exposure).Category("Sky");
        b.Property("skyTint", &EnvironmentComponent::skyTint).Category("Sky");
        // Ground & Ambient
        b.Property("groundColor", &EnvironmentComponent::groundColor).Category("Ground");
        b.Property("iblIntensity", &EnvironmentComponent::iblIntensity).Category("Ambient");
        return b.Build();
    }();
    return info;
}

}  // namespace Luma
