#version 450

// G-Buffer vertex shader — transforms model-space positions and normals
// to clip/world space for the geometry pass of the deferred pipeline.
// Matches the same vertex layout as scene.vert (MeshVertex: position + normal).

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

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    vWorldPos = worldPos.xyz;

    // Transform normal to world space (using the model matrix; assumes
    // uniform scale — for non-uniform scale use inverse-transpose).
    vNormal = normalize(mat3(pc.model) * inNormal);
    gl_Position = u.viewProj * worldPos;
}
