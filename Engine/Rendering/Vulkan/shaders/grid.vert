#version 450

// Infinite ground grid: a fullscreen triangle whose corners are unprojected to
// the near and far planes. The fragment shader intersects the Y=0 plane along
// the near->far segment, so the grid exists for the whole visible ground with
// no finite extent.

layout(push_constant) uniform GridPush {
    mat4 invViewProj;
    mat4 viewProj;
} pc;

layout(location = 0) out vec3 vNear;
layout(location = 1) out vec3 vFar;

vec3 unproject(vec2 ndc, float z) {
    vec4 p = pc.invViewProj * vec4(ndc, z, 1.0);
    return p.xyz / p.w;
}

void main() {
    vec2 ndc = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2)) *
                   2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    vNear = unproject(ndc, 0.0);  // near plane (Vulkan depth 0)
    vFar = unproject(ndc, 1.0);   // far plane  (Vulkan depth 1)
}
