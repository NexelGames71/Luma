#version 450

// Physically-based shading: Cook-Torrance direct lighting from the sun and any
// number of punctual lights (point/spot/directional) plus image-based lighting
// (diffuse irradiance + specular reflection) from the procedural sky. Output is
// tonemapped + gamma-encoded to match the sky pass on the UNORM target.

const int MAX_LIGHTS = 16;
const int MAX_CASCADES = 4;

struct Light {
    vec4 posType;   // xyz = position, w = type (0 dir, 1 point, 2 spot)
    vec4 dirRange;  // xyz = direction, w = range
    vec4 color;     // rgb = color, w = intensity
    vec4 spot;      // x = cosInner, y = cosOuter
};

layout(binding = 0) uniform SceneUBO {
    mat4 viewProj;
    vec4 camPos;
    vec4 camForward;  // xyz = camera forward (for cascade selection)
    vec4 sunDir;      // xyz dir TO sun
    vec4 sunColor;    // rgb, w = intensity
    vec4 skyZenith;
    vec4 skyHorizon;
    vec4 groundColor;
    vec4 params;         // x=iblIntensity, y=lightCount, z=sunShadows, w=1/size
    vec4 shadowParams;   // x=softness, y=cascadeCount
    vec4 cascadeSplits;  // per-cascade far view-depth
    mat4 cascadeViewProj[MAX_CASCADES];
    Light lights[MAX_LIGHTS];
} u;

layout(binding = 1) uniform sampler2DArray shadowMap;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 albedo;    // rgb, w = metallic
    vec4 material;  // x = roughness
} pc;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 0) out vec4 outColor;

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

// Percentage-closer soft shadows in one cascade layer.
float pcss(int cascade, vec3 proj, float bias) {
    vec2 uv = proj.xy * 0.5 + 0.5;
    float receiver = proj.z;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 ||
        receiver > 1.0 || receiver < 0.0) {
        return 1.0;
    }
    float texel = u.params.w;
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
float sunShadow(vec3 N, vec3 L) {
    if (u.params.z < 0.5) return 1.0;
    float viewDepth = dot(vWorldPos - u.camPos.xyz, u.camForward.xyz);
    int count = int(u.shadowParams.y);
    int cascade = count - 1;
    for (int i = 0; i < count; ++i) {
        if (viewDepth < u.cascadeSplits[i]) {
            cascade = i;
            break;
        }
    }
    vec4 sc = u.cascadeViewProj[cascade] * vec4(vWorldPos, 1.0);
    vec3 proj = sc.xyz / sc.w;
    // Larger bias for farther, coarser cascades.
    float bias = max(0.0018 * (1.0 - max(dot(N, L), 0.0)), 0.0004) *
                 (1.0 + float(cascade) * 0.6);
    return pcss(cascade, proj, bias);
}

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

// Cook-Torrance contribution for one light direction L (returns value to scale
// by the light's incoming radiance).
vec3 brdf(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float rough,
          vec3 F0) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float D = distributionGGX(N, H, rough);
    float G = geometrySmith(N, V, L, rough);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    vec3 spec = (D * G * F) / max(4.0 * NdotV * NdotL, 1e-4);
    vec3 kd = (vec3(1.0) - F) * (1.0 - metallic);
    return (kd * albedo / PI + spec) * NdotL;
}

vec3 skyEnv(vec3 dir) {
    float y = dir.y;
    vec3 upper = mix(u.skyHorizon.rgb, u.skyZenith.rgb, clamp(y, 0.0, 1.0));
    vec3 lower = mix(u.skyHorizon.rgb, u.groundColor.rgb, clamp(-y, 0.0, 1.0));
    return y >= 0.0 ? upper : lower;
}

vec3 irradiance(vec3 N) {
    vec3 sky = mix(u.skyHorizon.rgb, u.skyZenith.rgb,
                   clamp(N.y * 0.5 + 0.5, 0.0, 1.0));
    return mix(u.groundColor.rgb, sky, clamp(N.y * 0.5 + 0.75, 0.0, 1.0));
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(u.camPos.xyz - vWorldPos);
    float NdotV = max(dot(N, V), 1e-4);

    vec3 albedo = pc.albedo.rgb;
    float metallic = clamp(pc.albedo.w, 0.0, 1.0);
    float rough = clamp(pc.material.x, 0.04, 1.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Sun (Environment) direct, shadowed.
    vec3 sunL = normalize(u.sunDir.xyz);
    vec3 direct = brdf(N, V, sunL, albedo, metallic, rough, F0) *
                  u.sunColor.rgb * u.sunColor.w * sunShadow(N, sunL);

    // Punctual lights.
    int count = int(u.params.y);
    for (int i = 0; i < count && i < MAX_LIGHTS; ++i) {
        Light lt = u.lights[i];
        int type = int(lt.posType.w);
        vec3 L;
        float atten = 1.0;
        if (type == 0) {
            L = normalize(-lt.dirRange.xyz);  // directional
        } else {
            vec3 toL = lt.posType.xyz - vWorldPos;
            float dist = length(toL);
            L = toL / max(dist, 1e-4);
            float range = max(lt.dirRange.w, 1e-3);
            float rf = clamp(1.0 - pow(dist / range, 4.0), 0.0, 1.0);
            atten = (rf * rf) / max(dist * dist, 1e-4);
            if (type == 2) {  // spot cone
                float cosA = dot(-L, normalize(lt.dirRange.xyz));
                atten *= smoothstep(lt.spot.y, lt.spot.x, cosA);
            }
        }
        if (atten <= 0.0) continue;
        vec3 radiance = lt.color.rgb * lt.color.w * atten;
        direct += brdf(N, V, L, albedo, metallic, rough, F0) * radiance;
    }

    // IBL ambient.
    vec3 Fr = fresnelSchlickRough(NdotV, F0, rough);
    vec3 kdIBL = (vec3(1.0) - Fr) * (1.0 - metallic);
    vec3 diffuseIBL = irradiance(N) * albedo * kdIBL;

    vec3 R = reflect(-V, N);
    vec3 prefiltered = mix(skyEnv(R), irradiance(N), rough);
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 rp = rough * c0 + c1;
    float a004 = min(rp.x * rp.x, exp2(-9.28 * NdotV)) * rp.x + rp.y;
    vec2 envBRDF = vec2(-1.04, 1.04) * a004 + rp.zw;
    vec3 specularIBL = prefiltered * (F0 * envBRDF.x + envBRDF.y);

    vec3 ambient = (diffuseIBL + specularIBL) * u.params.x;
    vec3 color = direct + ambient;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
