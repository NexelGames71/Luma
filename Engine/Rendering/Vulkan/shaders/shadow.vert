#version 450

// Depth-only pass from the sun's point of view. `mvp` is lightViewProj * model.
// Only position is consumed (the pipeline binds a single attribute).
layout(location = 0) in vec3 inPos;

layout(push_constant) uniform Push {
    mat4 mvp;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
}
