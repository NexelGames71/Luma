#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

const int MAX_LIGHTS = 16;
const int MAX_CASCADES = 4;

struct Light {
    vec4 posType;
    vec4 dirRange;
    vec4 color;
    vec4 spot;
};

layout(binding = 0) uniform SceneUBO {
    mat4 viewProj;
    vec4 camPos;
    vec4 camForward;
    vec4 sunDir;
    vec4 sunColor;
    vec4 skyZenith;
    vec4 skyHorizon;
    vec4 groundColor;
    vec4 params;
    vec4 shadowParams;
    vec4 cascadeSplits;
    mat4 cascadeViewProj[MAX_CASCADES];
    Light lights[MAX_LIGHTS];
} u;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 albedo;
    vec4 material;
} pc;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;

void main() {
    vec4 world = pc.model * vec4(inPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = mat3(pc.model) * inNormal;
    gl_Position = u.viewProj * world;
}
