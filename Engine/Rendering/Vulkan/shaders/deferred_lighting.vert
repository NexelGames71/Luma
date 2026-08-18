#version 450

// Fullscreen triangle vertex shader — generates a single triangle that
// covers the entire screen without any vertex buffer.  Used by the
// deferred lighting and composition passes.
//
// Invoke with vkCmdDraw(3, 1, 0, 0) — no vertex input required.

layout(location = 0) out vec2 vUV;

void main() {
    // Generates a fullscreen triangle:
    //   gl_VertexIndex 0 → (-1, -1)  uv (0, 0)
    //   gl_VertexIndex 1 → ( 3, -1)  uv (2, 0)
    //   gl_VertexIndex 2 → (-1,  3)  uv (0, 2)
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vUV = pos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
