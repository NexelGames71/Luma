#version 450

// Physically-based shading: Cook-Torrance direct lighting from the sun plus
// image-based lighting (diffuse irradiance + specular reflection) sampled from
// the procedural sky environment. Output is tonemapped + gamma-encoded to match
// the sky pass on the UNORM target.

layout(binding = 0) uniform SceneUBO {
    mat4 viewProj;
    vec4 camPos;
    vec4 sunDir;      // xyz dir TO sun
    vec4 sunColor;    // rgb, w = intensity
    vec4 skyZenith;   // rgb IBL up
    vec4 skyHorizon;  // rgb IBL horizon
    vec4 groundColor; // rgb IBL down
    vec4 params;      // x = iblIntensity
} u;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 albedo;    // rgb, w = metallic
    vec4 material;  // x = roughness
} pc;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265358979323846;

float distributionGGX(vec3 N, vec3 H, float rough) {
    float a = rough * rough;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-7);
}

float geometrySchlickGGX(float NdotV, float rough) {
    float r = rough + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float rough) {
    return geometrySchlickGGX(max(dot(N, V), 0.0), rough) *
           geometrySchlickGGX(max(dot(N, L), 0.0), rough);
}

vec3 fresnelSchlick(float cosT, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosT, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRough(float cosT, vec3 F0, float rough) {
    vec3 Fr = max(vec3(1.0 - rough), F0);
    return F0 + (Fr - F0) * pow(clamp(1.0 - cosT, 0.0, 1.0), 5.0);
}

// Cheap analytic environment sky in a direction: blends ground/horizon/zenith
// by elevation. Stands in for a prefiltered environment map (IBL source).
vec3 skyEnv(vec3 dir) {
    float y = dir.y;
    vec3 upper = mix(u.skyHorizon.rgb, u.skyZenith.rgb, clamp(y, 0.0, 1.0));
    vec3 lower = mix(u.skyHorizon.rgb, u.groundColor.rgb, clamp(-y, 0.0, 1.0));
    return y >= 0.0 ? upper : lower;
}

// Hemispherical diffuse irradiance about N (integrated environment, approx).
vec3 irradiance(vec3 N) {
    vec3 sky = mix(u.skyHorizon.rgb, u.skyZenith.rgb,
                   clamp(N.y * 0.5 + 0.5, 0.0, 1.0));
    return mix(u.groundColor.rgb, sky, clamp(N.y * 0.5 + 0.75, 0.0, 1.0));
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(u.camPos.xyz - vWorldPos);
    vec3 L = normalize(u.sunDir.xyz);
    vec3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotL = max(dot(N, L), 0.0);

    vec3 albedo = pc.albedo.rgb;
    float metallic = clamp(pc.albedo.w, 0.0, 1.0);
    float rough = clamp(pc.material.x, 0.04, 1.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Direct: sun.
    float D = distributionGGX(N, H, rough);
    float G = geometrySmith(N, V, L, rough);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    vec3 spec = (D * G * F) / max(4.0 * NdotV * NdotL, 1e-4);
    vec3 kd = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 sunRadiance = u.sunColor.rgb * u.sunColor.w;
    vec3 direct = (kd * albedo / PI + spec) * sunRadiance * NdotL;

    // IBL: diffuse irradiance + specular reflection from the sky environment.
    vec3 Fr = fresnelSchlickRough(NdotV, F0, rough);
    vec3 kdIBL = (vec3(1.0) - Fr) * (1.0 - metallic);
    vec3 diffuseIBL = irradiance(N) * albedo * kdIBL;

    vec3 R = reflect(-V, N);
    // Blur the reflection toward the diffuse irradiance as roughness rises.
    vec3 prefiltered = mix(skyEnv(R), irradiance(N), rough);
    // Analytic environment BRDF (Karis mobile approximation).
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = rough * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    vec2 envBRDF = vec2(-1.04, 1.04) * a004 + r.zw;
    vec3 specularIBL = prefiltered * (F0 * envBRDF.x + envBRDF.y);

    vec3 ambient = (diffuseIBL + specularIBL) * u.params.x;

    vec3 color = direct + ambient;

    // Tonemap (Reinhard) + gamma, matching the sky pass.
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
