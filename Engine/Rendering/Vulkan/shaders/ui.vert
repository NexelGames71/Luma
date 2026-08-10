#version 450

// Luma Slate UI vertex shader. Positions are in screen-space pixels; converted
// to NDC using the display size from a push constant.

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;  // R8G8B8A8_UNORM -> normalized

layout(push_constant) uniform Push {
    vec2 screenSize;
} pc;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    vec2 ndc = (inPos / pc.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = inUV;
    vColor = inColor;
}
