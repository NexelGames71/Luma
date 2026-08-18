#version 450

// Material fragment shader TEMPLATE — the material editor's node graph is
// compiled to GLSL expressions and substituted into this file at runtime
// (see VulkanSceneView::BuildMaterialShader). Tokens:
//   __UNIFORMS__     extra uniform declarations the expressions reference
//   __BASE_COLOR__   base color (vec3 expression)
//   __METALLIC__     metallic (float)
//   __ROUGHNESS__    roughness (float)
//   __SPECULAR__     specular F0 multiplier (float)
//   __NORMAL__       world-space normal (vec3)
//   __EMISSIVE__     emissive color (vec3)
//   __OPACITY__      opacity (float)
//   __AMBIENT_OCCLUSION__  AO multiplier (float)
// The shading body is the same PBR core as scene.frag.

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
    vec4 shadowParams;   // x=softness, y=cascadeCount, z=shadowBias, w=normalBias
    vec4 cascadeSplits;  // per-cascade far view-depth
    mat4 cascadeViewProj[MAX_CASCADES];
    Light lights[MAX_LIGHTS];
    vec4 rayFlags;    // x=isCamera, y=isShadow, z=isDiffuse, w=isGlossy
    vec4 rayFlags2;   // x=isSingular, y=isReflection, z=isTransmission, w=isVolumeScatter
    vec4 rayDepths;   // x=rayDepth, y=diffuseDepth, z=glossyDepth, w=transparentDepth
    vec4 rayDepths2;  // x=transmissionDepth, y=portalDepth
    vec4 timeParams;  // x = elapsed time (uTime)
    mat4 view;        // camera view matrix (Texture Coordinate > Camera)
    vec4 viewSize;    // xy = viewport size (Texture Coordinate > Window)
} u;

// The compiler's uTime expression resolves to the scene clock.
#define uTime (u.timeParams.x)

layout(binding = 1) uniform sampler2DArray shadowMap;

// Scene instances for the Raycast node: the renderer uploads the scene's
// instances (model matrices + primitive types + self index) each frame.
layout(binding = 2) uniform InstanceBuffer {
    mat4 models[16];
    ivec4 prims[16];  // x = primitive (0 cube, 1 plane, 2 sphere, 3 cylinder), y = self index
    vec4 bboxMin[16]; // object-space AABB min (Texture Coordinate > Generated)
    vec4 bboxMax[16]; // object-space AABB max
    vec4 volume[16];  // xyz = smoke color, w = density (Volume Info node)
    vec4 volume2[16]; // x = flame, y = temperature (Volume Info node)
    vec4 count;       // x = instance count
} uInst;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 albedo;     // rgb, w = metallic
    vec4 material;   // x = roughness
    vec4 objColor;   // rgb = viewport display color, a = alpha
    int objIndex;    // object pass index
    int matIndex;    // material pass index
    float objRandom; // per-object random in [0,1]
    float pad;       // align p0 to 16 bytes
    // Particle Info node data (pushed after the object block).
    vec4 p0;         // x = index, y = random, z = age, w = lifetime
    vec4 p1;         // x = size
    vec4 location;   // particle world location
    vec4 velocity;
    vec4 angular;    // angular velocity
    // Point Info node data (point-cloud points).
    vec4 pt0;        // x = radius, y = random
    vec4 ptLoc;      // point world location
} pc;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec3 vTangent;
layout(location = 4) in vec3 vObjPos;
layout(location = 5) in vec4 vObjColor;
layout(location = 6) flat in int vObjIndex;
layout(location = 7) flat in int vObjMatIndex;
layout(location = 8) flat in float vObjRandom;
layout(location = 9) in vec3 vLocalPos;
layout(location = 10) in vec3 vLocalNormal;
layout(location = 11) in vec4 vVertexColor;
layout(location = 12) in vec3 vBarycentric;
layout(location = 0) out vec4 outColor;

__UNIFORMS__

// --- Wireframe node ----------------------------------------------------------
// Recovers each fragment's distance to the nearest triangle edge from the
// interpolated barycentric coordinate (stamped per corner by the renderer's
// de-indexing pass). Edge thickness is either screen-space pixels (Pixel
// Size) or world units (converted via the fragment's world-space
// derivative). Returns 1 on edges, 0 in face interiors.
float WireframeFactor(vec3 bc, float size, float pixelSize) {
    if (pixelSize > 0.5) {
        // Screen-space: size is in pixels; fwidth converts barycentric
        // units to pixel space per fragment.
        vec3 w = fwidth(bc);
        vec3 a = smoothstep(vec3(0.0), w * max(size, 1e-4), bc);
        return 1.0 - min(min(a.x, a.y), a.z);
    }
    // World-space: size in world units. The world-space extent of one
    // barycentric unit varies per fragment; derive it from the position
    // derivative so the thickness is constant in world space.
    float perBc = max(length(fwidth(vWorldPos)) / max(fwidth(bc).x, 1e-6), 1e-6);
    float w = max(size / perBc, 1e-4);
    vec3 a = smoothstep(vec3(0.0), vec3(w), bc);
    return 1.0 - min(min(a.x, a.y), a.z);
}

// --- Raycast node ------------------------------------------------------------
// Traces a ray against the scene's instances (uploaded in uInst) and reports
// the nearest hit. Primitives are intersected in each instance's local space
// and transformed back to world, so scaled instances give correct distances.

struct RaycastResult {
    float isHit;
    float selfHit;
    float distance;
    vec3 position;
    vec3 normal;
};

// Ray vs one canonical local-space primitive (all are 0.5 radius/half-size).
bool RayLocal(int prim, vec3 ro, vec3 rd, float tmax, out float t, out vec3 n) {
    t = tmax;
    n = vec3(0.0);
    if (prim == 0) {  // Cube: AABB [-0.5, 0.5]^3
        vec3 t0 = (vec3(-0.5) - ro) / rd;
        vec3 t1 = (vec3(0.5) - ro) / rd;
        vec3 tmin = min(t0, t1);
        vec3 tmax3 = max(t0, t1);
        float tn = max(max(tmin.x, tmin.y), tmin.z);
        float tf = min(min(tmax3.x, tmax3.y), tmax3.z);
        if (tf < tn || tf < 0.0 || tn > t) return false;
        if (tn == tmin.x) n = vec3(-sign(rd.x), 0.0, 0.0);
        else if (tn == tmin.y) n = vec3(0.0, -sign(rd.y), 0.0);
        else n = vec3(0.0, 0.0, -sign(rd.z));
        t = tn;
        return true;
    }
    if (prim == 1) {  // Plane: y = 0 quad, |x| <= 0.5, |z| <= 0.5
        if (abs(rd.y) < 1e-6) return false;
        float tq = -ro.y / rd.y;
        if (tq < 1e-4 || tq > t) return false;
        vec3 p = ro + rd * tq;
        if (abs(p.x) > 0.5 || abs(p.z) > 0.5) return false;
        t = tq;
        n = vec3(0.0, sign(rd.y), 0.0);  // face the ray approaches
        return true;
    }
    if (prim == 2) {  // Sphere radius 0.5
        float a = dot(rd, rd);
        float b = 2.0 * dot(ro, rd);
        float c = dot(ro, ro) - 0.25;
        float disc = b * b - 4.0 * a * c;
        if (disc < 0.0) return false;
        float sq = sqrt(disc);
        float hit = (-b - sq) / (2.0 * a);
        if (hit < 1e-4) hit = (-b + sq) / (2.0 * a);
        if (hit < 1e-4 || hit > t) return false;
        vec3 p = ro + rd * hit;
        n = p * 2.0;
        t = hit;
        return true;
    }
    // Cylinder: radius 0.5, y in [-0.5, 0.5] (side + caps).
    float a2 = rd.x * rd.x + rd.z * rd.z;
    if (a2 > 1e-8) {
        float b = 2.0 * (ro.x * rd.x + ro.z * rd.z);
        float c = ro.x * ro.x + ro.z * ro.z - 0.25;
        float disc = b * b - 4.0 * a2 * c;
        if (disc >= 0.0) {
            float sq = sqrt(disc);
            float ht = (-b - sq) / (2.0 * a2);
            if (ht >= 1e-4 && ht <= t) {
                float y = ro.y + rd.y * ht;
                if (y >= -0.5 && y <= 0.5) {
                    vec3 p = ro + rd * ht;
                    n = vec3(p.x, 0.0, p.z) * 2.0;
                    t = ht;
                    return true;
                }
            }
        }
    }
    if (abs(rd.y) > 1e-6) {
        for (int k = 0; k < 2; ++k) {
            float capY = k == 0 ? 0.5 : -0.5;
            float ht = (capY - ro.y) / rd.y;
            if (ht < 1e-4 || ht > t) continue;
            vec2 p = vec2(ro.x + rd.x * ht, ro.z + rd.z * ht);
            if (dot(p, p) <= 0.25) {
                t = ht;
                n = vec3(0.0, sign(capY), 0.0);
                return true;
            }
        }
    }
    return false;
}

RaycastResult RaycastScene(vec3 origin, vec3 dir, float maxLen, float selfOnly) {
    RaycastResult res;
    res.isHit = 0.0;
    res.selfHit = 0.0;
    res.distance = maxLen;
    res.position = vec3(0.0);
    res.normal = vec3(0.0);
    float best = maxLen;
    int bestIdx = -1;
    vec3 bestPos = vec3(0.0);
    vec3 bestN = vec3(0.0);
    int selfIdx = int(pc.pad + 0.5);
    int n = int(uInst.count.x);
    for (int i = 0; i < n; ++i) {
        int instSelf = uInst.prims[i].y;
        if (selfOnly > 0.5 && instSelf != selfIdx) continue;
        vec3 lo = (inverse(uInst.models[i]) * vec4(origin, 1.0)).xyz;
        vec3 ld = normalize((inverse(uInst.models[i]) * vec4(dir, 0.0)).xyz);
        float t;
        vec3 ln;
        if (RayLocal(uInst.prims[i].x, lo, ld, 1e30, t, ln)) {
            vec3 lp = lo + ld * t;
            vec3 wp = (uInst.models[i] * vec4(lp, 1.0)).xyz;
            float d = length(wp - origin);
            if (d > 1e-4 && d < best) {
                best = d;
                bestIdx = i;
                bestPos = wp;
                bestN = normalize(mat3(uInst.models[i]) * ln);
            }
        }
    }
    if (bestIdx >= 0) {
        res.isHit = 1.0;
        res.selfHit = bestIdx == selfIdx ? 1.0 : 0.0;
        res.distance = best;
        res.position = bestPos;
        res.normal = bestN;
    }
    return res;
}

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

float pcss(int cascade, vec3 proj, float bias) {
    vec2 uv = proj.xy * 0.5 + 0.5;
    float receiver = proj.z;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 ||
        receiver > 1.0 || receiver < 0.0) {
        return 1.0;
    }
    float texel = u.params.w;
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
    float bias = max(0.0018 * (1.0 - max(dot(N, L), 0.0)), 0.0004) *
                 (1.0 + float(cascade) * 0.6);
    bias = max(bias, u.shadowParams.z);
    proj.xy -= (N.xy / max(abs(proj.z), 1e-4)) * u.shadowParams.w;
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
    // Node-graph property overrides (substituted at runtime; the default
    // constants keep the shader valid if an expression is empty).
    vec3 N = normalize(__NORMAL__);
    vec3 albedo = clamp(__BASE_COLOR__, 0.0, 1.0);
    float metallic = clamp(__METALLIC__, 0.0, 1.0);
    float rough = clamp(__ROUGHNESS__, 0.04, 1.0);
    float specular = clamp(__SPECULAR__, 0.0, 1.0);
    vec3 emissive = __EMISSIVE__;
    float ao = clamp(__AMBIENT_OCCLUSION__, 0.0, 1.0);

    vec3 V = normalize(u.camPos.xyz - vWorldPos);
    float NdotV = max(dot(N, V), 1e-4);
    vec3 F0 = mix(vec3(0.04), albedo, metallic) * (1.0 + (specular - 0.5) * 0.2);

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
            if (type == 2) {
                float cosA = dot(-L, normalize(lt.dirRange.xyz));
                atten *= smoothstep(lt.spot.y, lt.spot.x, cosA);
            }
        }
        if (atten <= 0.0) continue;
        vec3 radiance = lt.color.rgb * lt.color.w * atten;
        direct += brdf(N, V, L, albedo, metallic, rough, F0) * radiance;
    }

    // IBL ambient (scaled by the AO factor).
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

    vec3 ambient = (diffuseIBL + specularIBL) * u.params.x * ao;
    vec3 color = direct + ambient + emissive;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, clamp(__OPACITY__, 0.0, 1.0));
}
