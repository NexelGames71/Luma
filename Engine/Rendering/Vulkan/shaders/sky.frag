#version 450

// Procedural sky background. Drawn as a single fullscreen triangle before the
// scene geometry (depth test/write disabled), so opaque shapes overwrite it.
// The vertex shader unprojects each corner through inv(view*proj) to hand the
// fragment shader a world-space homogeneous point it turns into a view ray.
//
// Sky color comes from the physically based single-scattering atmosphere in
// sky_atmosphere.glsl — the same model the deferred lighting pass uses for its
// background, so the forward and deferred paths match exactly.

#include "sky_atmosphere.glsl"

layout(push_constant) uniform SkyPush {
    mat4 invViewProj;
    vec4 cameraPos;  // xyz = world camera position
} pc;

layout(set = 0, binding = 0) uniform AtmosphereUBO {
    AtmosphereParams params;
} u;

layout(location = 0) in vec4 vWorldHomog;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 worldPos = vWorldHomog.xyz / vWorldHomog.w;
    outColor = vec4(RenderSkyColor(worldPos, pc.cameraPos.xyz, u.params), 1.0);
}
