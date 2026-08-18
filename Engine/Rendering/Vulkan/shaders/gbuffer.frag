#version 450

// G-Buffer fragment shader — writes per-pixel surface attributes to
// multiple render targets for the deferred lighting pass.
//
// RT0 (outAlbedo):    RGB = base colour,   A = unused
// RT1 (outNormal):    RGB = world normal,  A = roughness
// RT2 (outMaterial):  R = metallic,  G = specular, B = AO, A = unused
//
// Material texture maps (binding 1) are sampled per-instance: pc.texIdx
// carries indices into the texture array (-1 = no map → constant fallback).

layout(binding = 0) uniform CameraUBO {
    mat4 viewProj;
    vec4 camPos;
} u;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 albedo;    // rgb, w = metallic
    vec4 material;  // x = roughness
    ivec4 texIdx;   // baseColor, normal, roughness, metallic (-1 = none)
} pc;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterial;

void main() {
    vec3 N = normalize(vNormal);
    // Base color: map overrides / tints the constant.
    vec3 baseColor = pc.albedo.rgb;

    // Roughness + metallic: map replaces the constant when present.
    float roughness = pc.material.x;
    float metallic = pc.albedo.w;
    // Normal map: tangent-space normal → world via the TBN built from the
    // interpolated tangent. Bitangent is reconstructed with the right-hand
    // rule (uniform scale assumed, like the normal transform above).

    // RT0: albedo
    outAlbedo = vec4(baseColor, 1.0);

    // RT1: world-space normal ([-1,1] → [0,1] encoding) + roughness
    outNormal = vec4(N * 0.5 + 0.5, roughness);

    // RT2: metallic / specular / AO
    outMaterial = vec4(metallic,
                       0.5,          // specular  (default 0.5 = dielectric F0)
                       1.0,          // AO        (default 1 = fully lit)
                       0.0);
}
