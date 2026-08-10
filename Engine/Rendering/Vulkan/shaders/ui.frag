#version 450

// Samples a texture and modulates by the vertex color. Solid quads sample a 1x1
// white texture (so color passes through); text samples the font atlas whose
// alpha is glyph coverage.

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(set = 0, binding = 0) uniform sampler2D uTex;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vColor * texture(uTex, vUV);
}
