#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

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

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;

void main() {
    vec4 world = pc.model * vec4(inPos, 1.0);
    vWorldPos = world.xyz;
    // Uniform/rotation scale assumed; mat3(model) carries orientation.
    vNormal = mat3(pc.model) * inNormal;
    gl_Position = u.viewProj * world;
}
