#version 450

// Deferred lighting fragment shader — reads G-Buffer targets and computes
// Cook-Torrance PBR lighting, outputting the final lit colour.  This
// shader doubles as the composition pass: it samples the G-Buffer,
// evaluates lighting, applies tone mapping, and writes gamma-encoded
// colour to the output render target.
//
// G-Buffer inputs (set 0):
//   binding 0: RT0 — albedo (RGB)
//   binding 1: RT1 — normal (RGB encoded [0,1]) + roughness (A)
//   binding 2: RT2 — metallic (R) / specular (G) / AO (B)
//   binding 3: depth
//
// Lighting UBO (set 0, binding 4):
//   Sun direction, colour, ambient, shadow params, etc. Background fragments
//   (depth == far plane) render the physically based atmosphere from
//   sky_atmosphere.glsl — the same model the forward sky pass uses.

#include "sky_atmosphere.glsl"

const int MAX_LIGHTS = 16;
const int MAX_CASCADES = 4;
const float PI = 3.14159265359;

struct Light {
    vec4 posType;   // xyz = position, w = type (0 dir, 1 point, 2 spot)
    vec4 dirRange;  // xyz = direction, w = range
    vec4 color;     // rgb = color, w = intensity
    vec4 spot;      // x = cosInner, y = cosOuter
};

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gMaterial;
layout(set = 0, binding = 3) uniform sampler2D gDepth;

layout(set = 0, binding = 4) uniform LightingUBO {
    mat4  invViewProj;
    vec4  camPos;
    vec4  camForward;   // xyz = camera forward (for cascade selection)
    vec4  sunDir;       // xyz = direction TO sun
    vec4  sunColor;     // rgb, w = intensity
    vec4  ambientColor; // rgb, w = intensity
    vec4  params;       // x = lightCount, y = sunShadows, z = 1/shadowSize
    vec4  shadowParams; // x = softness, y = cascadeCount, z = shadowBias
    vec4  cascadeSplits;
    mat4  cascadeViewProj[MAX_CASCADES];
    AtmosphereParams atmo;
    Light lights[MAX_LIGHTS];
} u;

layout(set = 0, binding = 5) uniform sampler2DArray shadowMap;

// Poisson disk for the PCSS blocker search + filtering.
const vec2 POISSON[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790));

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

// ---- PBR helpers -----------------------------------------------------------

float DistributionGGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom + 0.0001);
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float g1 = NdotV / (NdotV * (1.0 - k) + k);
    float g2 = NdotL / (NdotL * (1.0 - k) + k);
    return g1 * g2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Reconstruct world position from depth + UV using inverse VP.
vec3 ReconstructWorldPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = u.invViewProj * clip;
    return world.xyz / world.w;
}

// Percentage-closer soft shadows in one cascade layer (matches scene.frag).
float pcss(int cascade, vec3 proj, float bias) {
    vec2 uv = proj.xy * 0.5 + 0.5;
    float receiver = proj.z;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 ||
        receiver > 1.0 || receiver < 0.0) {
        return 1.0;
    }
    float texel = u.params.z;
    // Blocker search: average depth of occluders in a light-sized region.
    float searchR = u.shadowParams.x * texel * 6.0;
    float blockerSum = 0.0;
    int blockers = 0;
    for (int i = 0; i < 16; ++i) {
        float d = texture(shadowMap, vec3(uv + POISSON[i] * searchR, cascade)).r;
        if (d < receiver - bias) {
            blockerSum += d;
            ++blockers;
        }
    }
    if (blockers == 0) return 1.0;
    float avgBlocker = blockerSum / float(blockers);
    // Penumbra grows with receiver-blocker separation (PCSS estimate).
    float penumbra = (receiver - avgBlocker) / max(avgBlocker, 1e-4);
    float filterR = clamp(penumbra * u.shadowParams.x * texel * 30.0, texel,
                          texel * 12.0);
    float sum = 0.0;
    for (int i = 0; i < 16; ++i) {
        float d = texture(shadowMap, vec3(uv + POISSON[i] * filterR, cascade)).r;
        sum += (receiver - bias > d) ? 0.0 : 1.0;
    }
    return sum / 16.0;
}

// Cascaded sun shadow: pick a cascade by view depth, then PCSS it.
float sunShadow(vec3 worldPos, vec3 N, vec3 L) {
    if (u.params.y < 0.5) return 1.0;
    float viewDepth = dot(worldPos - u.camPos.xyz, u.camForward.xyz);
    int count = int(u.shadowParams.y);
    int cascade = count - 1;
    for (int i = 0; i < count; ++i) {
        if (viewDepth < u.cascadeSplits[i]) {
            cascade = i;
            break;
        }
    }
    vec4 sc = u.cascadeViewProj[cascade] * vec4(worldPos, 1.0);
    vec3 proj = sc.xyz / sc.w;
    // Larger bias for farther, coarser cascades.
    float bias = max(0.0018 * (1.0 - max(dot(N, L), 0.0)), 0.0004) *
                 (1.0 + float(cascade) * 0.6);
    bias = max(bias, u.shadowParams.z);
    return pcss(cascade, proj, bias);
}

// ---- Main ------------------------------------------------------------------

void main() {
    // Sample G-Buffer
    vec3  albedo    = texture(gAlbedo, vUV).rgb;
    vec4  normalR   = texture(gNormal, vUV);
    vec4  matData   = texture(gMaterial, vUV);
    float depth     = texture(gDepth, vUV).r;

    // Decode normal from [0,1] → [-1,1]
    vec3 N = normalize(normalR.rgb * 2.0 - 1.0);
    float roughness = normalR.a;
    float metallic  = matData.r;
    float ao        = matData.b;

    // Background pixels (depth == far plane): render the procedural sky instead
    // of a flat color. When the sky is disabled (skyIntensity == 0) fall back to
    // the flat editor background.
    if (depth >= 0.9999) {
        if (u.atmo.ozoneSky.y > 0.0) {
            vec3 worldPos = ReconstructWorldPos(vUV, depth);
            outColor = vec4(RenderSkyColor(worldPos, u.camPos.xyz, u.atmo), 1.0);
        } else {
            outColor = vec4(0.07, 0.08, 0.10, 1.0);
        }
        return;
    }

    vec3 worldPos = ReconstructWorldPos(vUV, depth);
    vec3 V = normalize(u.camPos.xyz - worldPos);

    // Fresnel F0 — dielectrics use 0.04, metals use albedo.
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ---- Directional light (sun) ----
    vec3 L = normalize(u.sunDir.xyz);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    vec3  F = FresnelSchlick(HdotV, F0);

    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    vec3 sunContrib = (diffuse + specular) * u.sunColor.rgb * u.sunColor.w *
                      NdotL * sunShadow(worldPos, N, L);

    // ---- Punctual lights ----
    vec3 punctualContrib = vec3(0.0);
    int lightCount = int(u.params.x);
    for (int i = 0; i < lightCount && i < MAX_LIGHTS; ++i) {
        Light light = u.lights[i];
        vec3 lDir;
        float attenuation = 1.0;

        if (light.posType.w < 0.5) {
            // Directional
            lDir = normalize(light.dirRange.xyz);
        } else {
            // Point / spot
            vec3 toLight = light.posType.xyz - worldPos;
            float dist = length(toLight);
            lDir = toLight / dist;
            float range = light.dirRange.w;
            attenuation = max(1.0 - (dist * dist) / (range * range), 0.0);
            attenuation *= attenuation;

            if (light.posType.w > 1.5 && light.posType.w < 2.5) {
                // Spot
                float theta = dot(lDir, normalize(light.dirRange.xyz));
                float inner = light.spot.x;
                float outer = light.spot.y;
                attenuation *= clamp((theta - outer) / (inner - outer), 0.0, 1.0);
            }
            // Tube (type 3) behaves as a point light for now.
        }

        vec3 lH = normalize(V + lDir);
        float lNdotL = max(dot(N, lDir), 0.0);
        float lNdotH = max(dot(N, lH), 0.0);
        float lHdotV = max(dot(lH, V), 0.0);

        float lD = DistributionGGX(lNdotH, roughness);
        float lG = GeometrySmith(NdotV, lNdotL, roughness);
        vec3  lF = FresnelSchlick(lHdotV, F0);

        vec3 lSpec = (lD * lG * lF) / (4.0 * NdotV * lNdotL + 0.0001);
        vec3 lkD = (1.0 - lF) * (1.0 - metallic);
        vec3 lDiff = lkD * albedo / PI;

        punctualContrib += (lDiff + lSpec) * light.color.rgb * light.color.w *
                           lNdotL * attenuation;
    }

    // ---- Ambient ----
    vec3 ambient = u.ambientColor.rgb * u.ambientColor.w * albedo * ao;

    // ---- Compose ----
    vec3 color = sunContrib + punctualContrib + ambient;

    // Reinhard tone mapping
    color = color / (color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
