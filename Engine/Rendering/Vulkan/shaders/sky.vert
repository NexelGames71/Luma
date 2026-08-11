#version 450

// Procedural sky background. Drawn as a single fullscreen triangle before the
// scene geometry (depth test/write disabled), so opaque shapes overwrite it.
// The vertex shader unprojects each corner through inv(view*proj) to hand the
// fragment shader a world-space homogeneous point it turns into a view ray.

layout(push_constant) uniform SkyPush {
    mat4 invViewProj;
    vec4 cameraPos;  // xyz = world camera position
    vec4 sunDir;     // xyz = dir TO sun (normalized), w = below-horizon fade
    vec4 params;     // x=turbidity, y=sunIntensity, z=cosSunRadius, w=skyIntensity
    vec4 ground;     // rgb = color below horizon
} pc;

layout(location = 0) out vec4 vWorldHomog;

void main() {
    // Fullscreen-triangle trick: 3 verts cover NDC with 25% overdraw.
    vec2 ndc = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2)) *
                   2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    // Reconstruct the world ray at the far plane (z = 1 in 0..1 depth).
    vWorldHomog = pc.invViewProj * vec4(ndc, 1.0, 1.0);
}
